#! /usr/bin/python3

import make_helper
from make_helper import SpecializedEnvVarOption
from make_helper import SpecializedCMakeOption
from make_helper import SpecializedOptionType

specialized_opts = [
    SpecializedCMakeOption(config_var='make-paraview-reader', help_str='Build the ParaView reader (only applies to fea_io)',
                           option_type=SpecializedOptionType.CMakeBool,
                           modules_to_vars_map={'fea_io': ['SHEFF_BuildParaViewReader']}),
    
    SpecializedCMakeOption(config_var='use-mpi', help_str='Build with MPI (requires MPI environment variables for CMake to find it)',
                           option_type=SpecializedOptionType.CMakeBool,
                           modules_to_vars_map={'fea_io': ['SHEFF_USE_MPI']}),
]

make_helper.make(specialized_opts, root_repo_name='fea_io')
