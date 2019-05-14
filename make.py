import make_helper
from make_helper import SpecializedCMakeOption
from make_helper import CMakeOptionType

specialized_opts = [
    SpecializedCMakeOption(config_var='make-paraview-reader', help_str='Build the ParaView reader (only applies to fea_io)', \
        modules_and_var_map={ 'fea_io': 'FEAIO_BuildParaViewReader' }),
    SpecializedCMakeOption(config_var='use-mpi', help_str='Build with MPI (requires MPI environment variables for CMake to find it)', \
        modules_and_var_map={ 'fea_io': 'FEAIO_USE_MPI' }),
    SpecializedCMakeOption(config_var='use-fs', help_str='Turn on if compiler has <experimental/filesystem>', \
        modules_and_var_map={ 'fea_io': 'FEAIO_HaveFilesystem', 'utility': 'UTILITY_HaveFilesystem' })
]

make_helper.make(specialized_opts, build_order=dependencies.get_build_order())