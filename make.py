#! /usr/bin/python3

import make_helper
from make_helper import SpecializedEnvVarOption
from make_helper import SpecializedCMakeOption
from make_helper import SpecializedOptionType

specialized_opts = [
    SpecializedCMakeOption(config_var='make-paraview-reader', help_str='Build the ParaView reader (only applies to fea_io)', \
        modules_and_vars_map={ 'fea_io': ['FEAIO_BuildParaViewReader'] }),
    SpecializedCMakeOption(config_var='use-mpi', help_str='Build with MPI (requires MPI environment variables for CMake to find it)', \
        modules_and_vars_map={ 'fea_io': ['FEAIO_USE_MPI'] }),
    SpecializedCMakeOption( config_var='use-fs-hack', help_str='Turn on if compiler does not provide <experimental/filesystem>', \
                            modules_and_vars_map={ 'fea_io': ['FEAIO_UseFilesystemHack'] } ),
]

make_helper.make( specialized_opts )
