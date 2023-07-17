
#include "CUFLU.h"
#include "GAMER.h"


#if ( MODEL == ELBDM && WAVE_SCHEME == WAVE_GRAMFE && GRAMFE_SCHEME == GRAMFE_MATMUL )

#include "GramFE_ExtensionTables.h"

#include <complex.h>

#define GRAMFE_EV_FLOAT8

#ifdef GRAMFE_EV_FLOAT8
#define gramfe_evo_float __float128
#else // #ifdef GRAMFE_EV_FLOAT8
#define gramfe_evo_float __float128
#endif // #ifdef GRAMFE_EV_FLOAT8 ... # else


using gramfe_matmul_complex_type = std::complex<gramfe_matmul_float>;
using gramfe_evo_complex_type    = std::complex<gramfe_evo_float>;

//-------------------------------------------------------------------------------------------------------
// Function    :  Factorial
// Description :  Compute the factorial of n.
// Parameter   :  n  : integer
// Return      :  n!
//-------------------------------------------------------------------------------------------------------
int Factorial(int n) {
     return (n==0) || (n==1) ? 1 : n* Factorial(n-1);
} // FUNCTION : Factorial

//-------------------------------------------------------------------------------------------------------
// Function    :  CosineTaylorExpansion
// Description :  Compute the taylor expansion of the cosine function at the point x with Nterms terms
// Parameter   :  x       : Point at which to evaluate Taylor expansion
//                NTerms  : Number of terms to retain in expansion
// Return      :  Value of expansion
//-------------------------------------------------------------------------------------------------------
gramfe_evo_float CosineTaylorExpansion(gramfe_evo_float x, int Nterms) {
   gramfe_evo_float result = 0;

   for (int i = 0; i  < Nterms; ++i) {
      result += pow(-1, i) * (1 / ((gramfe_evo_float) Factorial(2 * i))) * pow((long double)x, 2 * i   );
   }

   return result;
} // FUNCTION : CosineTaylorExpansion

//-------------------------------------------------------------------------------------------------------
// Function    :  SineTaylorExpansion
// Description :  Compute the taylor expansion of the sine function at the point x with Nterms terms
// Parameter   :  x       : Point at which to evaluate Taylor expansion
//                NTerms  : Number of terms to retain in expansion
// Return      :  Value of expansion
//-------------------------------------------------------------------------------------------------------
gramfe_evo_float SineTaylorExpansion(gramfe_evo_float x, int Nterms) {
   gramfe_evo_float result = 0;

   for (int i = 0; i  < Nterms; ++i) {
      result += pow(-1, i) * ( 1 / ((gramfe_evo_float) Factorial(2 * i + 1)) ) * pow((long double) x, 2 * i + 1);
   }

   return result;
} // FUNCTION : SineTaylorExpansion


//-------------------------------------------------------------------------------------------------------
// Function    :  GramFE_ComputeTimeEvolutionMatrix
// Description :  Compute the time evolution matrix for the Schrödinger equation and store result in output
// Parameter   :  output  : Complex PS2 x 2 * FLU_NXT matrix (contiguous memory block of size 2 * FLU_NXT * PS2 * sizeof(gramfe_matmul_float) bytes)
//                dt      : Time step
//                dh      : Grid spacing
//                Eta     : m/hbar
//-------------------------------------------------------------------------------------------------------
void ELBDM_GramFE_ComputeTimeEvolutionMatrix(gramfe_matmul_float (*output)[2 * FLU_NXT], real dt, real dh, real Eta)
{
// set up time evolution operator and filter
   gramfe_evo_float K, Filter, Coeff;
   gramfe_evo_complex_type ExpCoeff;

   const gramfe_evo_float filterDecay  = (gramfe_evo_float) 32.0 * (gramfe_evo_float) 2.302585092994046; // decay of k-space filter ( 32 * log(10) )
   const gramfe_evo_float filterDegree = (gramfe_evo_float) 100;                                       // degree of k-space filter
   const gramfe_evo_float kmax         = (gramfe_evo_float) M_PI / dh;                                 // maximum value of k
   const gramfe_evo_float dk           = (gramfe_evo_float) + 2.0 * kmax / GRAMFE_FLU_NXT;             // k steps in k-space
   const gramfe_evo_float dT           = (gramfe_evo_float) - 0.5 * dt / Eta;                          // coefficient in time evolution operator

// naively "exp(1j * Coeff)" should give the exact time evolution of the free Schrödinger equation
// however, the time-evolution operator depends on higher-order derivatives
// since these are unavailable for a finite ghost boundary size, the series needs to be truncated
// the ideal Taylor expansion order using all available derivatives is FLU_GHOST_SIZE - 1
// for FLU_GHOST_SIZE == 8, 4 terms in the cosine series and 3 terms in the sine series are retained
   const int cosineNTerms = FLU_GHOST_SIZE / 2;
   const int sineNTerms   = cosineNTerms - (int) ((FLU_GHOST_SIZE % 2) == 0);

// set up momentum, filter and time evolution array
   gramfe_matmul_complex_type (* Out)    [FLU_NXT]        = (gramfe_matmul_complex_type (*)[       FLU_NXT]) output;
   gramfe_evo_complex_type    (* FFT)    [GRAMFE_FLU_NXT] = (gramfe_evo_complex_type    (*)[GRAMFE_FLU_NXT]) GramFE_FFT;
   gramfe_evo_complex_type    (*IFFT)    [GRAMFE_FLU_NXT] = (gramfe_evo_complex_type    (*)[GRAMFE_FLU_NXT]) GramFE_IFFT;
   gramfe_evo_complex_type    (*Extend)  [FLU_NXT]        = (gramfe_evo_complex_type    (*)[       FLU_NXT]) GramFE_Extend;

   gramfe_evo_complex_type DFFT     [GRAMFE_FLU_NXT][GRAMFE_FLU_NXT];  //        exp(-i k^2 dt) * FFT
   gramfe_evo_complex_type IFFTDFFT [           PS2][GRAMFE_FLU_NXT];  // IFFT * exp(-i k^2 dt) * FFT
   gramfe_evo_complex_type Evolution[           PS2][       FLU_NXT];  // IFFT * exp(-i k^2 dt) * FFT * extension

   for (int i=0; i<GRAMFE_FLU_NXT; i++)
   {
      K           = ( i <= GRAMFE_FLU_NXT/2 ) ? dk*i : dk*(i-GRAMFE_FLU_NXT);
      Filter      = exp(-(long double) filterDecay * pow((long double) abs(K/kmax), 2*filterDegree));
      Coeff       = SQR(K)*dT;
      ExpCoeff    = gramfe_evo_complex_type(CosineTaylorExpansion(Coeff, cosineNTerms), SineTaylorExpansion(Coeff, sineNTerms)) * Filter;

//    multiply FFT matrix by diagonal time evolution operator in k-space
      for (int k=0; k<GRAMFE_FLU_NXT; k++)
      {
         DFFT[i][k] = ExpCoeff * FFT[i][k];
      }
   } // for (int i=0; i<GRAMFE_FLU_NXT; i++)

   for ( int i = 0; i < PS2; ++i )
   for ( int j = 0; j < GRAMFE_FLU_NXT; ++j )
   {
      gramfe_evo_complex_type out = {0, 0};
      for ( int k = 0; k < GRAMFE_FLU_NXT; ++k )
      {
         out += IFFT[i][k]  * DFFT[k][j];
      }
      IFFTDFFT[i][j] = out;
   }

   for ( int i = 0; i < PS2; ++i )
   for ( int j = 0; j < FLU_NXT; ++j )
   {
      gramfe_evo_complex_type out = {0, 0};
      for ( int k = 0; k < GRAMFE_FLU_NXT; ++k )
      {
         out += IFFTDFFT[i][k]  * Extend[k][j];
      }
      Evolution[i][j] = out;
   }

   for (size_t i = 0; i < PS2; ++i) {
      for (size_t j = 0; j < FLU_NXT; ++j) {
         Out[i][j].real((gramfe_matmul_float) Evolution[i][j].real());
         Out[i][j].imag((gramfe_matmul_float) Evolution[i][j].imag());
      }
   }
}

#endif // #if ( MODEL == ELBDM && WAVE_SCHEME == WAVE_GRAMFE && GRAMFE_SCHEME == GRAMFE_MATMUL )