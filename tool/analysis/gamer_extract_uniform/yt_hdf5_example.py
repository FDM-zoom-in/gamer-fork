
# ref: https://yt-project.org/docs/dev/examining/generic_array_data.html

import yt
import h5py
import numpy as np


# user-specified parameters
file_in = 'Cube_x0.067-0.087_y0.067-0.097_z0.067-0.107_lv1_000001.hdf5'    # input filename
fields  = [ 'Dens', ]                                                      # target fields
units   = [ 'code_mass/code_length**3', ]                                  # units of all target fields


# load data
f         = h5py.File( file_in, 'r' )
dimension = f['Info']['GridDimension']
time      = f['Info']['Time']
box_size  = f['Info']['SubdomainSize']
left_edge = f['Info']['SubdomainLeftEdge']
unit_l    = ( f['Info']['Unit_L'], 'cm' )
unit_m    = ( f['Info']['Unit_M'], 'g' )
unit_t    = ( f['Info']['Unit_T'], 's' )

bbox = np.array( [ [left_edge[0], left_edge[0]+box_size[0]],
                   [left_edge[1], left_edge[1]+box_size[1]],
                   [left_edge[2], left_edge[2]+box_size[2]] ] )
data = { k:(f['Data'][k][()].transpose(),u) for k,u in zip(fields,units) }

ds = yt.load_uniform_grid( data=data, domain_dimensions=dimension,
                           length_unit=unit_l, mass_unit=unit_m, time_unit=unit_t,
                           bbox=bbox, nprocs=1, sim_time=time,
                           periodicity=(False,False,False) )


# plot
sz = yt.SlicePlot( ds, 'z', fields, center='c' )
sz.save()