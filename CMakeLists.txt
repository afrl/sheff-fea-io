cmake_policy(SET CMP0048 NEW)

project(fea_io VERSION 1.0.0 LANGUAGES CXX)

cmake_minimum_required(VERSION 3.8)

# Options
option(FEA_IO_BuildDev "Build the main.cpp in the dev folder. (intended for debugging this project by devs)" ON)
option(FEA_IO_BuildParaViewReader "Build ParaView reader for the JSON-binary format supported by the fea_io project. (Requires having build ParaView from source)" ON)
option(FEA_IO_BuildTests "Build the unit tests." ON)

# Guard against in-source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt.")
endif()

# Guard against empty build-type strings
if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

# Guard against invalid build-type strings
string(TOLOWER "${CMAKE_BUILD_TYPE}" cmake_build_type_tolower)
if(NOT cmake_build_type_tolower STREQUAL "debug"
   AND NOT cmake_build_type_tolower STREQUAL "release"
   AND NOT cmake_build_type_tolower STREQUAL "relwithdebinfo")
  message(FATAL_ERROR "Unknown build type \"${CMAKE_BUILD_TYPE}\". Allowed values are Debug, Release, RelWithDebInfo (case-insensitive).")
endif()

# Ensure we have C++ 11 support
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++11" FEA_IO_COMPILER_SUPPORT_CPP11)
if(NOT FEA_IO_COMPILER_SUPPORT_CPP11)
	message(FATAL_ERROR "The compiler must support C++ 11.")
endif()

# Find MPI
find_package(MPI)
if(MPI_FOUND)
	include_directories(${MPI_INCLUDE_PATH})
endif()

# Set the variables for fea_io
set(FEA_IO_TARGET_NAME ${PROJECT_NAME})
set(FEA_IO_INCLUDE ${CMAKE_SOURCE_DIR}/include)
set(FEA_IO_INCLUDE_SUB ${FEA_IO_INCLUDE}/fea_io)
set(FEA_IO_HEADER_LIST
	${FEA_IO_INCLUDE_SUB}/common/Common.h
	${FEA_IO_INCLUDE_SUB}/common/Exceptions.h
	${FEA_IO_INCLUDE_SUB}/common/IOUtils.h
	${FEA_IO_INCLUDE_SUB}/common/MPIUtils.h
	${FEA_IO_INCLUDE_SUB}/data/DataReader.h
	${FEA_IO_INCLUDE_SUB}/data/DataStructs.h
	${FEA_IO_INCLUDE_SUB}/data/DataWriter.h
	${FEA_IO_INCLUDE_SUB}/group/GroupReader.h
	${FEA_IO_INCLUDE_SUB}/group/GroupStructs.h
	${FEA_IO_INCLUDE_SUB}/group/GroupWriter.h
	${FEA_IO_INCLUDE_SUB}/mesh/MeshEnums.h
	${FEA_IO_INCLUDE_SUB}/mesh/MeshInfoStructs.h
	${FEA_IO_INCLUDE_SUB}/mesh/MeshIteratorAdapters.h
	${FEA_IO_INCLUDE_SUB}/mesh/MeshParWriter.h
	${FEA_IO_INCLUDE_SUB}/mesh/MeshReader.h
	${FEA_IO_INCLUDE_SUB}/mesh/MeshStructs.h
	${FEA_IO_INCLUDE_SUB}/mesh/MeshWriter.h)
set(FEA_IO_CONFIG_INSTALL_DIR "lib/cmake/${PROJECT_NAME}" CACHE INTERNAL "")
set(FEA_IO_INCLUDE_INSTALL_DIR "include")
set(FEA_IO_TARGETS_EXPORT_NAME "${PROJECT_NAME}Targets")
set(FEA_IO_CMAKE_CONFIG_TEMPLATE "cmake/config.cmake.in")
set(FEA_IO_CMAKE_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(FEA_IO_CMAKE_VERSION_CONFIG_FILE "${FEA_IO_CMAKE_CONFIG_DIR}/${PROJECT_NAME}ConfigVersion.cmake")
set(FEA_IO_CMAKE_PROJECT_CONFIG_FILE "${FEA_IO_CMAKE_CONFIG_DIR}/${PROJECT_NAME}Config.cmake")
set(FEA_IO_CMAKE_PROJECT_TARGETS_FILE "${FEA_IO_CMAKE_CONFIG_DIR}/${PROJECT_NAME}Targets.cmake")

# Create header-only target
add_library(${FEA_IO_TARGET_NAME} INTERFACE)
target_compile_features(${FEA_IO_TARGET_NAME} INTERFACE cxx_std_11)
target_include_directories(${FEA_IO_TARGET_NAME} INTERFACE ${FEA_IO_INCLUDE})
target_sources(${FEA_IO_TARGET_NAME} INTERFACE ${FEA_IO_HEADER_LIST})

# For targets that use FEA_IO they should call assign_source_group(${FEA_IO_HEADER_LIST})
# in order to group the files by the directories they are in within Visual Studio.
function(assign_source_group)
    foreach(_source IN ITEMS ${ARGN})
        if (IS_ABSOLUTE "${_source}")
            file(RELATIVE_PATH _source_rel "${FEA_IO_INCLUDE}" "${_source}")
        else()
            set(_source_rel "${_source}")
        endif()
        get_filename_component(_source_path "${_source_rel}" PATH)
		string(REPLACE "/" "\\" _source_path_msvc "${_source_path}")
        source_group("${_source_path_msvc}" FILES "${_source}")
    endforeach()
endfunction(assign_source_group)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Find JSON library
find_package(nlohmann_json REQUIRED)
target_link_libraries(${FEA_IO_TARGET_NAME} INTERFACE nlohmann_json::nlohmann_json)

# Build dev
if(FEA_IO_BuildDev)
	add_subdirectory(dev)
endif()

# Build paraview plugin
if(FEA_IO_BuildParaViewReader)
	add_subdirectory(paraview_plugin)
endif()

# Build tests
#include(CTest)
if(FEA_IO_BuildTests)
	enable_testing()
	add_subdirectory(test)
endif()

# Install
include(CMakePackageConfigHelpers)
write_basic_package_version_file(${FEA_IO_CMAKE_VERSION_CONFIG_FILE} COMPATIBILITY SameMajorVersion)
configure_file(
	${FEA_IO_CMAKE_CONFIG_TEMPLATE}
    ${FEA_IO_CMAKE_PROJECT_CONFIG_FILE}
    @ONLY
)
install(
	DIRECTORY ${FEA_IO_INCLUDE_SUB}
    DESTINATION ${FEA_IO_INCLUDE_INSTALL_DIR}
)
install(
    FILES ${FEA_IO_CMAKE_PROJECT_CONFIG_FILE} ${FEA_IO_CMAKE_VERSION_CONFIG_FILE}
    DESTINATION ${FEA_IO_CONFIG_INSTALL_DIR}
)
export(
    TARGETS ${FEA_IO_TARGET_NAME}
    NAMESPACE ${PROJECT_NAME}::
    FILE ${FEA_IO_CMAKE_PROJECT_TARGETS_FILE}
)
#install(
#    TARGETS ${FEA_IO_TARGET_NAME}
#    EXPORT ${FEA_IO_TARGETS_EXPORT_NAME}
#    INCLUDES DESTINATION ${FEA_IO_INCLUDE_INSTALL_DIR}
#)
#install(
#    EXPORT ${FEA_IO_TARGETS_EXPORT_NAME}
#    NAMESPACE ${PROJECT_NAME}::
#    DESTINATION ${FEA_IO_CONFIG_INSTALL_DIR}
#)