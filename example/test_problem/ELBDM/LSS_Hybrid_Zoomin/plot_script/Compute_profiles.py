#!/usr/bin/env python3.9
################################################################################

# This code outputs the density, cumulative mass, circular velocity and velocity dispersion profiles
# of a target halo. The code uses an initial estimate (center_first_guess) to search the vicinity within 
# a specified radius (vicinity) for the coordinates of the maximum density,
# corresponding to the center of the target halo.

# carefully adjust vicinity for performance.

# Example usage after running simulation with low resolution IC: 
#    python3 Compute_profiles -s 36 -e 36

# Outputs:
#   Halo_Parameter
#   prof_dens/Data_0000xx_0_profile_data
#   prof_mass/Data_0000xx_0_mass_accumule
#   prof_dens/Data_0000xx_0_profile_data
#   prof_circular_vel/Data_0000xx_0_circular_velocity
#   prof_veldisp/Data_0000xx_0_veldisp_haloRestFrame

################################################################################
import argparse
import numpy as np
import yt
from Profile_Functions import *

# load the command-line parameters
parser = argparse.ArgumentParser(description='Plot profile and out put Halo_parameter')

parser.add_argument('-s', action='store', required=True,  type=int, dest='idx_start',
                      help='first data index')
parser.add_argument('-e', action='store', required=True,  type=int, dest='idx_end',
                      help='last data index')
parser.add_argument('-d', action='store', required=False, type=int, dest='didx',
                      help='delta data index [%(default)d]', default=1)

args        = parser.parse_args()
idx_start   = args.idx_start
idx_end     = args.idx_end
didx        = args.didx

for file_id in range(idx_start, idx_end+1, 1):

    ds                 = yt.load("../Data_0000%.2d"%file_id)
    center_first_guess = np.array([0.295, 9.522, 8.27]) # in cMpc/h. First guess for target halo of low resolution IC at z=0
    vicinity           = 0.3   # radius in cMpc/h
    compute_profile(ds, center_first_guess, vicinity, 0, '.')
    
print("Done !")