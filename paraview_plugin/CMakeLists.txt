cmake_minimum_required(VERSION 3.8)
project(FEAIOReader)

set(BUILD_SHARED_LIBS ON)

include(GNUInstallDirs)
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")

find_package(Eigen3 3.3.0 CONFIG REQUIRED
  HINTS "${CMAKE_SOURCE_DIR}/../../third_party/eigen_build")
# Note: this has to go before fea_io
find_package(nlohmann_json 3.2.0 CONFIG REQUIRED
  HINTS "${CMAKE_SOURCE_DIR}/../../third_party/json_build"
        "G:/dev/bsam++/modules/third_party/json_build")
find_package(FeaIo CONFIG REQUIRED
  HINTS "${CMAKE_SOURCE_DIR}/../build"
        "${CMAKE_SOURCE_DIR}/../install_dir")

set(TBB_INCLUDE_DIR "C:/Program Files (x86)/IntelSWTools/compilers_and_libraries_2018.5.274/windows/tbb/include")
set(TBB_LIBRARY_RELEASE "C:/Program Files (x86)/IntelSWTools/compilers_and_libraries_2018.5.274/windows/tbb/lib/intel64_win/vc14/tbb.lib")
set(TBB_LIBRARY_DEBUG "C:/Program Files (x86)/IntelSWTools/compilers_and_libraries_2018.5.274/windows/tbb/lib/intel64_win/vc14/tbb_debug.lib")

find_package(ParaView REQUIRED
  HINTS "G:/dev/ParaView-v5.7.0_Build")

  set("_paraview_plugin_default_${CMAKE_PROJECT_NAME}" ON)
paraview_plugin_scan(
  PLUGIN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/plugin/paraview.plugin"
  PROVIDES_PLUGINS plugins
  REQUIRES_MODULES  required_modules)

foreach (module IN LISTS required_modules)
  if (NOT TARGET "${module}")
    message("Missing required module: ${module}")
    return ()
  endif ()
endforeach ()

paraview_plugin_build(
  RUNTIME_DESTINATION "${CMAKE_INSTALL_BINDIR}"
  LIBRARY_DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY_SUBDIRECTORY "${PARAVIEW_PLUGIN_SUBDIR}"
  PLUGINS ${plugins})