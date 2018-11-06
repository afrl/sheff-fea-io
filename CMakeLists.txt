project(fea_io VERSION 1.0.0 LANGUAGES CXX)

cmake_minimum_required(VERSION 3.8)

# Guard against in-source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt. ")
endif()

## Options
option(FEA_IO_BuildDev "Build the main.cpp in the dev folder. (intended for debugging this project by devs)" ON)
option(FEA_IO_BuildTests "Build the unit tests." ON)

# Find packages
# find_package(xxx REQUIRED QUIET)

# Imports
# import(xxx)
# if(NOT TARGET xxx)
# 	add_subdirectory("../..." "$(CMAKE_BINARY_DIR)/fea_io/xxx")
# endif()

## Configuration
set(FEA_IO_TARGET_NAME ${PROJECT_NAME})

# Build dev
if (FEA_IO_BuildDev)
	target_include_directories("")
	target_compile_definitiions()
	target_link_libraries()
endif (FEA_IO_BuildDev)

set_target_properties(fea_io PROPERTIES_FOLDER "fea_io")