set(FEA_IO_PV_READER_TARGET_NAME feaio_pv_reader)

#####################
# Find Dependencies #
#####################

find_package(ParaView REQUIRED)
find_package(Eigen3 REQUIRED HINTS ${CMAKE_BINARY_DIR}/../third_party/eigen)

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

   REQUIRED_ON_SERVER)

target_include_directories(FEAIOReader PRIVATE ${EIGEN3_INCLUDE_DIR})
target_link_libraries(FEAIOReader PRIVATE feaio)

include(${CMAKE_BINARY_DIR}/cmake/Utilities.cmake)
assign_source_group(${FEAIO_INCLUDE} ${FEAIO_HEADER_LIST})
