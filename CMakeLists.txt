cmake_policy(SET CMP0048 NEW)

project(fea_io VERSION 1.0.0 LANGUAGES CXX)

cmake_minimum_required(VERSION 3.8)

# Options
option(FEA_IO_BuildDev "Build the main.cpp in the dev folder. (intended for debugging this project by devs)" ON)
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

# Find packages
# find_package(xxx REQUIRED QUIET)

# Imports
# import(xxx)
# if(NOT TARGET xxx)
# 	add_subdirectory("../..." "$(CMAKE_BINARY_DIR)/fea_io/xxx")
# endif()

# Configure project wide variables
set(FEA_IO_INCLUDE ${CMAKE_SOURCE_DIR}/include)

# Find MPI
find_package(MPI)
if(MPI_FOUND)
	include_directories(${MPI_INCLUDE_PATH})
endif()

# Create header-only target
set(FEA_IO_TARGET_NAME ${PROJECT_NAME})
add_library(${FEA_IO_TARGET_NAME} INTERFACE)
target_compile_features(${FEA_IO_TARGET_NAME} INTERFACE cxx_std_11)
target_include_directories(${FEA_IO_TARGET_NAME} INTERFACE ${FEA_IO_INCLUDE})
target_sources(${FEA_IO_TARGET_NAME} INTERFACE
	${FEA_IO_INCLUDE}/common/Common.h
	${FEA_IO_INCLUDE}/common/Exceptions.h
	${FEA_IO_INCLUDE}/common/IOUtils.h
	${FEA_IO_INCLUDE}/common/MPIUtils.h
	${FEA_IO_INCLUDE}/mesh/MeshEnums.h
	${FEA_IO_INCLUDE}/mesh/MeshIteratorAdapters.h
	${FEA_IO_INCLUDE}/mesh/MeshParWriter.h
	${FEA_IO_INCLUDE}/mesh/MeshReader.h
	${FEA_IO_INCLUDE}/mesh/MeshStructs.h
	${FEA_IO_INCLUDE}/mesh/MeshWriter.h)

# Find JSON library
find_package(nlohmann_json REQUIRED)
target_link_libraries(${FEA_IO_TARGET_NAME} INTERFACE nlohmann_json::nlohmann_json)
#set_target_properties(nlohmann_json::nlohmann_json PROPERTIES INTERFACE_COMPILE_FEATURES ${COMPUTED_CXX_STD})

# Build dev
if(FEA_IO_BuildDev)
	add_subdirectory(dev)
endif()

# Build tests
include(CTest)
if(FEA_IO_BuildTests)
	enable_testing()
	add_subdirectory(test)
endif()

#set_target_properties(fea_io PROPERTIES_FOLDER "fea_io")