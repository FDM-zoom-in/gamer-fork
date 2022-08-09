#include "CUPOT.h"
#ifdef __CUDACC__
#include "CUDA_CheckError.h"
#endif

#ifdef GRAVITY



/********************************************************
1. Point-mass external potential
   --> It can be regarded as a template for implementing
       other external potential

2. This file is shared by both CPU and GPU

   GPU_Poisson/CUPOT_ExtPot_EridanusII.cu -> CPU_Poisson/CPU_ExtPot_EridanusII.cpp

3. Three steps are required to implement external potential

   I.   Set auxiliary arrays
        --> SetExtPotAuxArray_EridanusII()

   II.  Specify external potential
        --> ExtPot_EridanusII()

   III. Set initialization functions
        --> SetGPUExtPot_EridanusII()
            SetCPUExtPot_EridanusII()
            Init_ExtPot_EridanusII()

4. The external potential major routine, ExtPot_EridanusII(),
   must be thread-safe and not use any global variable

5. Reference: https://github.com/gamer-project/gamer/wiki/Gravity#external-accelerationpotential
********************************************************/



// =================================
// I. Set auxiliary arrays
// =================================

#ifndef __CUDACC__

extern bool    Tidal_RotatingFrame;
extern double  Tidal_Mass;
extern double  Tidal_R;
extern double  Tidal_Soften;
extern double  Tidal_Angle0;
extern bool    Tidal_FixedPos;
extern bool    Tidal_Centrifugal;
extern double  Tidal_Vrot;
extern double  Tidal_CM[3];

//-------------------------------------------------------------------------------------------------------
// Function    :  SetExtPotAuxArray_EridanusII
// Description :  Set the auxiliary arrays ExtPot_AuxArray_Flt/Int[] used by ExtPot_EridanusII()
//
// Note        :  1. Invoked by Init_ExtPot_EridanusII()
//                2. AuxArray_Flt/Int[] have the size of EXT_POT_NAUX_MAX defined in Macro.h (default = 20)
//                3. Add "#ifndef __CUDACC__" since this routine is only useful on CPU
//
// Parameter   :  AuxArray_Flt/Int : Floating-point/Integer arrays to be filled up
//                Time             : Target physical time
//
// Return      :  AuxArray_Flt/Int[]
//-------------------------------------------------------------------------------------------------------
void SetExtPotAuxArray_EridanusII( double AuxArray_Flt[], int AuxArray_Int[], const double Time )
{

   if ( Tidal_RotatingFrame )
   {
      AuxArray_Flt[0] = Tidal_CM[0];
      AuxArray_Flt[1] = Tidal_CM[1];
      AuxArray_Flt[2] = Tidal_CM[2];
   }

   else
   {
      AuxArray_Flt[0] = amr->BoxCenter[0];
      AuxArray_Flt[1] = amr->BoxCenter[1];
      AuxArray_Flt[2] = amr->BoxCenter[2];
   }

   AuxArray_Flt[ 3] = NEWTON_G*Tidal_Mass;
   AuxArray_Flt[ 4] = Tidal_R;
   AuxArray_Flt[ 5] = Tidal_Vrot;
   AuxArray_Flt[ 6] = ( Tidal_FixedPos ) ? +1.0 : -1.0;
   AuxArray_Flt[ 7] = ( Tidal_Centrifugal ) ? +1.0 : -1.0;
   AuxArray_Flt[ 8] = Tidal_Angle0;
   AuxArray_Flt[ 9] = ( Tidal_RotatingFrame ) ? +1.0 : -1.0;
   AuxArray_Flt[10] = Tidal_Soften;

} // FUNCTION : SetExtPotAuxArray_EridanusII
#endif // #ifndef __CUDACC__



// =================================
// II. Specify external potential
// =================================

//-----------------------------------------------------------------------------------------
// Function    :  ExtPot_EridanusII
// Description :  Calculate the external potential at the given coordinates and time
//
// Note        :  1. This function is shared by CPU and GPU
//                2. Auxiliary arrays UserArray_Flt/Int[] are set by SetExtPotAuxArray_EridanusII(), where
//                      UserArray_Flt[0] = x coordinate of the external potential center
//                      UserArray_Flt[1] = y ...
//                      UserArray_Flt[2] = z ..
//                      UserArray_Flt[3] = gravitational_constant*point_source_mass
//                3. Currently it does not support the soften length
//                4. GenePtr has the size of EXT_POT_NGENE_MAX defined in Macro.h (default = 6)
//
// Parameter   :  x/y/z             : Target spatial coordinates
//                Time              : Target physical time
//                UserArray_Flt/Int : User-provided floating-point/integer auxiliary arrays
//                Usage             : Different usages of external potential when computing total potential on level Lv
//                                    --> EXT_POT_USAGE_ADD     : add external potential on Lv
//                                        EXT_POT_USAGE_SUB     : subtract external potential for preparing self-gravity potential on Lv-1
//                                        EXT_POT_USAGE_SUB_TINT: like SUB but for temporal interpolation
//                                    --> This parameter is useless in most cases
//                PotTable          : 3D potential table used by EXT_POT_TABLE
//                GenePtr           : Array of pointers for general potential tables
//
// Return      :  External potential at (x,y,z,Time)
//-----------------------------------------------------------------------------------------
GPU_DEVICE_NOINLINE
static real ExtPot_EridanusII( const double x, const double y, const double z, const double Time,
                               const double UserArray_Flt[], const int UserArray_Int[],
                               const ExtPotUsage_t Usage, const real PotTable[], void **GenePtr )
{

   const bool RotatingFrame = ( UserArray_Flt[9] > 0.0 ) ? true : false;

   if ( RotatingFrame )
   {
      const double CM[3]       = { UserArray_Flt[0], UserArray_Flt[1], UserArray_Flt[2] };
      const real   GM          = UserArray_Flt[3];
      const real   R           = UserArray_Flt[4];
      const real   Vrot        = UserArray_Flt[5];
      const bool   FixedPos    = ( UserArray_Flt[6] > 0.0 ) ? true : false;
      const bool   Centrifugal = ( UserArray_Flt[7] > 0.0 ) ? true : false;
      const real   Angle0      = UserArray_Flt[8];

      real dx, dy, dz, dr2, _R, theta, Rx, Ry, Rz, tmp, phi;

//    calculate the relative coordinates between the target cell and the center of mass of the satellite
      dx  = (real)( x - CM[0] );
      dy  = (real)( y - CM[1] );
      dz  = (real)( z - CM[2] );
      dr2 = SQR(dx) + SQR(dy) + SQR(dz);

//    calculate the relative coordinates between the MW center and the center of mass of the satellite
//    --> assuming a circular orbit on the xy plane with an initial azimuthal angle of theta=0, where theta=acos(Rx/R)
      _R    = (real)1.0 / R;
      theta = ( FixedPos ) ? Angle0 : Vrot*Time*_R + Angle0;
      Rx    = R*COS( theta );
      Ry    = R*SIN( theta );
      Rz    = (real)0.0;
      tmp   = ( dx*Rx + dy*Ry + dz*Rz )*_R;
      phi   = (real)0.5*GM*CUBE(_R)*( dr2 - (real)3.0*SQR(tmp) );

      if ( Centrifugal )
      phi  -= (real)0.5*GM*CUBE(_R)*( SQR(dx) + SQR(dy) );

      return phi;
   } // if ( RotatingFrame )

   else
   {
      const double Cen[3] = { UserArray_Flt[0], UserArray_Flt[1], UserArray_Flt[2] };
      const real   GM     = (real)UserArray_Flt[3];
      const real   dx     = (real)(x - Cen[0]);
      const real   dy     = (real)(y - Cen[1]);
      const real   dz     = (real)(z - Cen[2]);
      const real   eps    = (real)UserArray_Flt[10];
      const real   _r     = (real)1.0/SQRT( SQR(dx) + SQR(dy) + SQR(dz) + SQR(eps) );

      return -GM*_r;
   } // if ( RotatingFrame ) ... else ...

} // FUNCTION : ExtPot_EridanusII



// =================================
// III. Set initialization functions
// =================================

#ifdef __CUDACC__
#  define FUNC_SPACE __device__ static
#else
#  define FUNC_SPACE            static
#endif

FUNC_SPACE ExtPot_t ExtPot_Ptr = ExtPot_EridanusII;

//-----------------------------------------------------------------------------------------
// Function    :  SetCPU/GPUExtPot_EridanusII
// Description :  Return the function pointers of the CPU/GPU external potential routines
//
// Note        :  1. Invoked by Init_ExtPot_EridanusII()
//                2. Must obtain the CPU and GPU function pointers by **separate** routines
//                   since CPU and GPU functions are compiled completely separately in GAMER
//                   --> In other words, a unified routine like the following won't work
//
//                      SetExtPot_EridanusII( ExtPot_t &CPUExtPot_Ptr, ExtPot_t &GPUExtPot_Ptr )
//
// Parameter   :  CPU/GPUExtPot_Ptr (call-by-reference)
//
// Return      :  CPU/GPUExtPot_Ptr
//-----------------------------------------------------------------------------------------
#ifdef __CUDACC__
__host__
void SetGPUExtPot_EridanusII( ExtPot_t &GPUExtPot_Ptr )
{
   CUDA_CHECK_ERROR(  cudaMemcpyFromSymbol( &GPUExtPot_Ptr, ExtPot_Ptr, sizeof(ExtPot_t) )  );
}

#else // #ifdef __CUDACC__

void SetCPUExtPot_EridanusII( ExtPot_t &CPUExtPot_Ptr )
{
   CPUExtPot_Ptr = ExtPot_Ptr;
}

#endif // #ifdef __CUDACC__ ... else ...



#ifndef __CUDACC__

// local function prototypes
void SetExtPotAuxArray_EridanusII( double [], int [], const double );
void SetCPUExtPot_EridanusII( ExtPot_t & );
#ifdef GPU
void SetGPUExtPot_EridanusII( ExtPot_t & );
#endif

//-----------------------------------------------------------------------------------------
// Function    :  Init_ExtPot_EridanusII
// Description :  Initialize external potential
//
// Note        :  1. Set auxiliary arrays by invoking SetExtPotAuxArray_*()
//                   --> They will be copied to GPU automatically in CUAPI_SetConstMemory()
//                2. Set the CPU/GPU external potential major routines by invoking SetCPU/GPUExtPot_*()
//                3. Invoked by Init_ExtAccPot()
//                   --> Enable it by linking to the function pointer "Init_ExtPot_Ptr"
//                4. Add "#ifndef __CUDACC__" since this routine is only useful on CPU
//
// Parameter   :  None
//
// Return      :  None
//-----------------------------------------------------------------------------------------
void Init_ExtPot_EridanusII()
{

   SetExtPotAuxArray_EridanusII( ExtPot_AuxArray_Flt, ExtPot_AuxArray_Int, Time[0] );
   SetCPUExtPot_EridanusII( CPUExtPot_Ptr );
#  ifdef GPU
   SetGPUExtPot_EridanusII( GPUExtPot_Ptr );
#  endif

} // FUNCTION : Init_ExtPot_EridanusII

#endif // #ifndef __CUDACC__



#endif // #ifdef GRAVITY
