cmake_policy(SET CMP0048 NEW)
cmake_minimum_required(VERSION 3.8)

project(FEAIO VERSION 1.0.0 LANGUAGES CXX)

# Options
option(FEAIO_BuildDev "Build the main.cpp in the dev folder. (intended for debugging this project by devs)" ON)
option(FEAIO_BuildParaViewReader "Build ParaView reader for the JSON-binary format supported by the FEAIO project. (Requires having build ParaView from source)" OFF)
option(FEAIO_BuildTests "Build the unit tests." ON)
option(FEAIO_USE_MPI "Indicates if the MPI routines should be included in the build. (Required for parallel writing)." OFF)

##################
# Checking input #
##################

include(${CMAKE_SOURCE_DIR}/cmake/CommonInputChecks.cmake)

########################
# Declare dependencies #
########################

find_package(nlohmann_json 3.2.0 CONFIG REQUIRED
	HINTS "${CMAKE_SOURCE_DIR}/../third_party/json/build" "${CMAKE_SOURCE_DIR}/../json/build"
	"${CMAKE_SOURCE_DIR}/../third_party/json/install_dir" "${CMAKE_SOURCE_DIR}/../json/install_dir"
)
if(FEAIO_USE_MPI)
	find_package(MPI REQUIRED)
endif()

#######################################
# Setting up FEAIO header-only target #
#######################################

# Set the variables for FEAIO
set(FEAIO_INCLUDE ${CMAKE_SOURCE_DIR}/include)
set(FEAIO_INCLUDE_SUB ${FEAIO_INCLUDE}/fea_io)
set(FEAIO_HEADER_LIST
	${FEAIO_INCLUDE_SUB}/common/Common.h
	${FEAIO_INCLUDE_SUB}/common/Exceptions.h
	${FEAIO_INCLUDE_SUB}/common/IOUtils.h
	${FEAIO_INCLUDE_SUB}/common/MPIUtils.h
	${FEAIO_INCLUDE_SUB}/data/DataReader.h
	${FEAIO_INCLUDE_SUB}/data/DataStructs.h
	${FEAIO_INCLUDE_SUB}/data/DataWriter.h
	${FEAIO_INCLUDE_SUB}/group/GroupReader.h
	${FEAIO_INCLUDE_SUB}/group/GroupStructs.h
	${FEAIO_INCLUDE_SUB}/group/GroupWriter.h
	${FEAIO_INCLUDE_SUB}/mesh/MeshEnums.h
	${FEAIO_INCLUDE_SUB}/mesh/MeshInfoStructs.h
	${FEAIO_INCLUDE_SUB}/mesh/MeshIteratorAdapters.h
	${FEAIO_INCLUDE_SUB}/mesh/MeshParWriter.h
	${FEAIO_INCLUDE_SUB}/mesh/MeshReader.h
	${FEAIO_INCLUDE_SUB}/mesh/MeshStructs.h
	${FEAIO_INCLUDE_SUB}/mesh/MeshWriter.h
	${FEAIO_INCLUDE_SUB}/meta/MetaReader.h)
	
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
add_library(FEAIO::FEAIO ALIAS feaio)
# Add C++ 11 flag (at interface level since this is a header-only
# library and downstream executables will also need the flag)
target_compile_features(feaio INTERFACE cxx_std_11)
# Link dependencies
target_link_libraries(feaio INTERFACE nlohmann_json::nlohmann_json)
if(FEAIO_USE_MPI AND MPI_FOUND)
    target_compile_definitions(feaio INTERFACE USE_MPI)
	target_include_directories(feaio INTERFACE ${MPI_CXX_INCLUDE_PATH})
    target_link_libraries(feaio INTERFACE ${MPI_CXX_LIBRARIES} ${MPI_CXX_LINK_FLAGS})
	target_compile_options(feaio INTERFACE ${MPI_CXX_COMPILE_FLAGS})
endif()

#######################################
# Add dev and ParaView reader targets #
#######################################

# Build dev
if(FEAIO_BuildDev)
	add_subdirectory(dev)
endif()

# Build paraview plugin
if(FEAIO_BuildParaViewReader)
	add_subdirectory(paraview_plugin)
endif()

# Build tests
#include(CTest)
if(FEAIO_BuildTests)
	enable_testing()
	add_subdirectory(test)
endif()

################
# Installation #
################

include(GNUInstallDirs)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/FEAIO)

# Install the header files
install(DIRECTORY ${FEAIO_INCLUDE}/
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
# This is required so that the exported target has the name FEAIO and not feaio
set_target_properties(feaio PROPERTIES EXPORT_NAME FEAIO)

# Create a ConfigVersion.cmake file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/FEAIOConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Create the package config file for find_package(FEAIO)
configure_package_config_file(
	${CMAKE_CURRENT_LIST_DIR}/cmake/FEAIOConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/FEAIOConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
)

# Install the config and config version files
# Note: if you write custom find modules, add them here
# Refer to https://github.com/pabloariasal/modern-cmake-sample/blob/cfda7b51ff915409fd61d4f2e75b63032bedb182/libjsonutils/CMakeLists.txt
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/FEAIOConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/FEAIOConfigVersion.cmake
	${CMAKE_CURRENT_BINARY_DIR}/FEAIOTargets.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)

###########
# Exports #
###########

# Export just the feaio target (not the dev, tests, ParaViewReader, etc.)
export(
    TARGETS feaio
	FILE ${CMAKE_CURRENT_BINARY_DIR}/FEAIOTargets.cmake
    NAMESPACE FEAIO::
)

# Register package in user's package registry
export(PACKAGE FEAIO)
