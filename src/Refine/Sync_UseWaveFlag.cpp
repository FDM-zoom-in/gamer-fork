#include "GAMER.h"



#if ( MODEL == ELBDM && ELBDM_SCHEME == ELBDM_HYBRID )

//-------------------------------------------------------------------------------------------------------
// Function    :  Sync_UseWaveFlag
// Description :  Sync amr->use_wave_flag across all GAMER ranks
//
// Note        :  None
//
// Parameter   :  lv : Target refinement level
//-------------------------------------------------------------------------------------------------------
void Sync_UseWaveFlag( const int lv )
{

   if ( lv < 0  ||  lv > TOP_LEVEL )
      Aux_Error( ERROR_INFO, "incorrect parameter %s = %d !!\n", "lv", lv );

    int recv;
    int send = amr->use_wave_flag[lv];

    MPI_Allreduce(&send, &recv, 1, MPI_INT, MPI_LOR, MPI_COMM_WORLD);

    amr->use_wave_flag[lv] = recv;
} // FUNCTION : Sync_UseWaveFlag

#endif // #if ( MODEL == ELBDM && ELBDM_SCHEME == ELBDM_HYBRID )