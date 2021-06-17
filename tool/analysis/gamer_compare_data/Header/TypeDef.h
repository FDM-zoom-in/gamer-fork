#ifndef __TYPEDEF_H__
#define __TYPEDEF_H__



#ifdef FLOAT8
typedef double real;
#else
typedef float  real;
#endif


// models
#define HYDRO              1
#define ELBDM              3


#ifndef NULL
#define NULL               0
#endif


#ifndef __FLT_MIN__
#  define __FLT_MIN__      1.17549435e-38F
#endif


// patch size (number of cells of a single patch in the x/y/z directions)
#define PATCH_SIZE         8
#define PS1                PATCH_SIZE
#define PS1P1              ( PS1 + 1 )


// number of components in each cell and the variable indices in the array "fluid"
#if   ( MODEL == HYDRO )
#  define NCOMP_FLUID      5
#  define NCOMP_MAG        3
#  define DENS             0
#  define MOMX             1
#  define MOMY             2
#  define MOMZ             3
#  define ENGY             4

#elif ( MODEL == ELBDM )
#  define NCOMP_FLUID      3
#  define DENS             0
#  define REAL             1
#  define IMAG             2

#else
#  error : ERROR : unsupported MODEL !!
#endif // MODEL

#  define NCOMP_TOTAL      ( NCOMP_FLUID + NCOMP_PASSIVE )


// useful macros
#define SQR(  a )       ( (a)*(a)     )
#define CUBE( a )       ( (a)*(a)*(a) )

#define MAX( a, b )     (  ( (a) > (b) ) ? (a) : (b)  )
#define MIN( a, b )     (  ( (a) < (b) ) ? (a) : (b)  )

#define IDX321_BX( i, j, k )   (  ( (k)*PS1   + (j) )*PS1P1 + (i)  )
#define IDX321_BY( i, j, k )   (  ( (k)*PS1P1 + (j) )*PS1   + (i)  )
#define IDX321_BZ( i, j, k )   (  ( (k)*PS1   + (j) )*PS1   + (i)  )


// macro for Aux_Error()
#define ERROR_INFO         __FILE__, __LINE__, __FUNCTION__


void Aux_Error( const char *File, const int Line, const char *Func, const char *Format, ... );




//-------------------------------------------------------------------------------------------------------
// Structure   :  patch_t
// Description :  data structure of a single patch
//
// Data Member :  fluid       : fluid variables (mass density, momentum density x, y ,z, energy density)
//                pot         : potential
//                par_dens    : particle density deposited onto grids
//                mag_cc/fc   : cell-/face-centered B field
//                corner[3]   : physical coordinates of the patch corner
//                father      : patch ID of the father patch
//                son         : patch ID of the child patch
//
// Method      :  patch_t     : constructor
//                ~patch_t    : destructor
//-------------------------------------------------------------------------------------------------------
struct patch_t
{

// data members
// ===================================================================================
   real (*fluid   )[PS1][PS1][PS1];
   real (*pot     )[PS1][PS1];
   real (*par_dens)[PS1][PS1];
   real (*mag_cc  )[PS1][PS1][PS1];
   real (*mag_fc  )[PS1P1*PS1*PS1];

   int  corner[3];
   int  father;
   int  son;
   bool check;



   //===================================================================================
   // Constructor :  patch_t
   // Description :  constructor of the structure "patch_t"
   //
   // Note        :  initialize data members
   //
   // Parameter   :  x,y,z : physical coordinates of the patch corner
   //                FaPID : patch ID of the father patch
   //                Data  : true --> allocate physical data (fluid + pot + par_dens + mag_cc/fc)
   //===================================================================================
   patch_t( const int x, const int y, const int z, const int FaPID, const bool Data )
   {
      corner[0] = x;
      corner[1] = y;
      corner[2] = z;
      father    = FaPID;
      son       = -1;
      check     = false;

      fluid    = NULL;
      pot      = NULL;
      par_dens = NULL;
      mag_cc   = NULL;
      mag_fc   = NULL;

      if ( Data )
      {
         fluid    = new real [NCOMP_TOTAL][PS1][PS1][PS1];
         pot      = new real              [PS1][PS1][PS1];
         par_dens = new real              [PS1][PS1][PS1];
         mag_cc   = new real [NCOMP_MAG  ][PS1][PS1][PS1];
         mag_fc   = new real [NCOMP_MAG  ][ PS1P1*SQR(PS1) ];
      }
   }

}; // struct patch_t





//-------------------------------------------------------------------------------------------------------
// Structure   :  AMR_t
// Description :  data structure of GAMER
//
// Data Member :  patch : pointer of each patch
//                num   : number of patches (real patch + buffer patch) at each level
//                scale : grid size at each level (normalize to the grid size at the finest level
//
// Method      :  pnew    : allocate one patch
//                pdelete : deallocate one patch
//-------------------------------------------------------------------------------------------------------
struct AMR_t
{

// data members
// ===================================================================================
   patch_t *patch[NLEVEL][MAX_PATCH];

   int num[NLEVEL];
   int scale[NLEVEL];
   int nx0_tot[3];



   //===================================================================================
   // Constructor :  AMR_t
   // Description :  constructor of the structure "AMR_t"
   //
   // Note        :  initialize data members
   //===================================================================================
   AMR_t()
   {
      for (int lv=0; lv<NLEVEL; lv++)
      {
         num  [lv] = 0;
         scale[lv] = 1<<(NLEVEL-1-lv);
      }

      for (int d=0; d<3; d++)
      {
         nx0_tot[d] = 0;
      }

      for (int lv=0; lv<NLEVEL; lv++)
      for (int PID=0; PID<MAX_PATCH; PID++)
         patch[lv][PID] = NULL;
   }



   //===================================================================================
   // Method      :  pnew
   // Description :  allocate a single patch
   //
   // Parameter   :  lv    : the targeted refinement level
   //                x,y,z : physical coordinates of the patch corner
   //                FaPID : the patch ID of the parent patch at level "lv-1"
   //                Data  : true --> allocate physical data (fluid + pot + par_dens + mag_cc/fc)
   //===================================================================================
   void pnew( const int lv, const int x, const int y, const int z, const int FaPID, const bool Data )
   {
      if ( patch[lv][num[lv]] != NULL )
         Aux_Error( ERROR_INFO, "allocate an existing patch (Lv %d, PID %d, FaPID %d) !!\n",
                    lv, num[lv], FaPID );

      patch[lv][ num[lv] ] = new patch_t( x, y, z, FaPID, Data );

      num[lv] ++;

      if ( num[lv] > MAX_PATCH )
         Aux_Error( ERROR_INFO, "exceed MAX_PATCH !!\n" );
   }

}; // struct AMR_t



#endif // #ifndef __TYPEDEF_H__

