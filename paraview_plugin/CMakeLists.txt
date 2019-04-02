set(FEA_IO_PV_READER_TARGET_NAME feaio_pv_reader)

#####################
# Find Dependencies #
#####################

find_package(ParaView REQUIRED)
find_package(Eigen3 3.3.0 CONFIG REQUIRED HINTS "${CMAKE_SOURCE_DIR}/../third_party/eigen/install_dir" "${CMAKE_SOURCE_DIR}/../eigen/install_dir")

#################
# Create Target #
#################

include(${PARAVIEW_USE_FILE})
add_paraview_plugin(FEAIOReader "1.0" 
   SERVER_MANAGER_XML src/FEAIOReader.xml
   SERVER_MANAGER_SOURCES src/vtkFEAIOReader.cxx 

   SOURCES src/ArrayHelpers.h
           src/ConnectivityHelpers.h
           src/PVReaderStructs.h
           src/vtkFEAIOReader.h

   REQUIRED_ON_SERVER)

target_include_directories(FEAIOReader PUBLIC src)
target_link_libraries(FEAIOReader PUBLIC Eigen3::Eigen)
target_link_libraries(FEAIOReader PRIVATE FeaIo::FeaIo)

include(${CMAKE_SOURCE_DIR}/cmake/Utilities.cmake)
#assign_source_group(${FEAIO_INCLUDE} ${FEAIO_HEADER_LIST})
