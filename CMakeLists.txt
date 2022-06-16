cmake_minimum_required(VERSION 3.8)

cmake_policy(SET CMP0048 NEW)
project(FEAIO VERSION 1.0.0 LANGUAGES CXX)

# Options
option( SHEFF_BuildParaViewReader "Build ParaView reader for the JSON-binary format supported by the FEAIO project. (Requires having build ParaView from source)" OFF )
option( SHEFF_BuildTests "Build the unit tests." OFF )
option( SHEFF_USE_MPI "Indicates if the MPI routines should be included in the build. (Required for parallel writing)." OFF )
option( SHEFF_UseFilesystemHack "Use if compiler does not have <experimental/filesystem>" OFF )

# For debugging, print generator, compiler, and options
message( STATUS "Gen: ${CMAKE_GENERATOR}, CXX: ${CMAKE_CXX_COMPILER_ID}, Mode: ${CMAKE_BUILD_TYPE}" )
message( STATUS "Tests: ${SHEFF_BuildTests}, MPI: ${SHEFF_USE_MPI}, ParaView: ${SHEFF_BuildParaViewReader}, FS HACK: ${SHEFF_UseFilesystemHack}" )

# Add cmake dir to module path
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

##################
# Checking input #
##################

include( ${PROJECT_SOURCE_DIR}/cmake/CommonInputChecks.cmake )

########################
# Declare dependencies #
########################

find_package(nlohmann_json 3.2.0 CONFIG REQUIRED HINTS 
             "${PROJECT_SOURCE_DIR}/../../third_party/json_install_dir")

if( SHEFF_USE_MPI )
    find_package(MPI REQUIRED COMPONENTS CXX)
endif()

#######################################
# Setting up FEAIO header-only target #
#######################################

# Set the variables for FEAIO
set(FEAIO_INCLUDE ${PROJECT_SOURCE_DIR}/include)
file (GLOB_RECURSE FEAIO_HEADER_LIST "${FEAIO_INCLUDE}/*.h")
        
# Create header-only target
add_library(feaio INTERFACE)
# Add the header includes and headers to the library
target_include_directories(feaio INTERFACE
        $<BUILD_INTERFACE:${FEAIO_INCLUDE}>
        $<INSTALL_INTERFACE:include>
)
# Sources are only header files, and this is at the interface level so
# downstream targets will have these headers in the VS project
target_sources(feaio INTERFACE ${FEAIO_HEADER_LIST})
# Add an alias so that a library can be used inside the build tree, eg when testing
add_library(FeaIo::FeaIo ALIAS feaio)
# Add C++ 17 flag (at interface level since this is a header-only
# library and downstream executables will also need the flag)
target_compile_features(feaio INTERFACE cxx_std_17)

# Use filesystem if available
if( SHEFF_UseFilesystemHack )
else()
  target_compile_definitions(feaio INTERFACE HAVE_FILESYSTEM )
  if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" )
      target_link_libraries( feaio INTERFACE libstdc++fs.a )
  endif()
  if (MSVC)
      # This is to silence the error that MS will remove <experimental/filesystem> and replace
      # it with <filesystem>
      target_compile_definitions(feaio INTERFACE _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING)
  endif()
endif()

# Link dependencies
target_link_libraries(feaio INTERFACE nlohmann_json::nlohmann_json)

if(SHEFF_USE_MPI AND MPI_FOUND)
    target_compile_definitions(feaio INTERFACE USE_MPI)
        target_include_directories(feaio INTERFACE ${MPI_CXX_INCLUDE_PATH})
    target_link_libraries(feaio INTERFACE ${MPI_CXX_LIBRARIES} ${MPI_CXX_LINK_FLAGS})
        target_compile_options(feaio INTERFACE ${MPI_CXX_COMPILE_FLAGS})
endif()

###########################
# Assigning source groups #
###########################

include("$ENV{SHEFF_TOOLS}/cmake/IncludeSHEFF.cmake")
register_sources_sheff("fea_io"
                       "${FEAIO_HEADER_LIST}"
                       "${FEAIO_SOURCE_LIST}")
assign_source_groups_sheff()

#######################################
# Add unit test and ParaView reader targets #
#######################################

# Build paraview plugin
if(SHEFF_BuildParaViewReader)
        add_subdirectory(paraview_plugin)
endif()

# Build tests
if(SHEFF_BuildTests)
        include(CTest)
        enable_testing()
        add_subdirectory(unit_tests)
endif()

################
# Installation #
################

include(GNUInstallDirs)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/FeaIo)

# Install the header files
install(DIRECTORY ${FEAIO_INCLUDE}/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
# This is required so that the exported target has the name FeaIo and not feaio
set_target_properties(feaio PROPERTIES EXPORT_NAME FeaIo)

# Create a ConfigVersion.cmake file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/FeaIoConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Create the package config file for find_package(FeaIo)
configure_package_config_file(
        ${CMAKE_CURRENT_LIST_DIR}/cmake/FeaIoConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/FeaIoConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
)

# Install the config and config version files
# Note: if you write custom find modules, add them here
# Refer to https://github.com/pabloariasal/modern-cmake-sample/blob/cfda7b51ff915409fd61d4f2e75b63032bedb182/libjsonutils/CMakeLists.txt
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/FeaIoConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/FeaIoConfigVersion.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/FeaIoTargets.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)

###########
# Exports #
###########

# Export just the feaio target (not tests, ParaViewReader, etc.)
export(
    TARGETS feaio
        FILE ${CMAKE_CURRENT_BINARY_DIR}/FeaIoTargets.cmake
    NAMESPACE FeaIo::
)
