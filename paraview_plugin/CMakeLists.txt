set(FEA_IO_PV_READER_TARGET_NAME ${PROJECT_NAME}_pv_reader)

find_package(ParaView REQUIRED)
include(${PARAVIEW_USE_FILE})
add_paraview_plugin(FEAIOReader "1.0" 
   SERVER_MANAGER_XML FEAIOReader.xml
   SERVER_MANAGER_SOURCES vtkFEAIOReader.cxx 

   SOURCES ArrayHelpers.h
           ConnectivityHelpers.h
           PVReaderStructs.h

   REQUIRED_ON_SERVER)

find_package(Eigen3 REQUIRED)
include_directories(${EIGEN3_INCLUDE_DIR})
   
target_link_libraries(FEAIOReader PRIVATE ${FEA_IO_TARGET_NAME})
assign_source_group(${FEA_IO_HEADER_LIST})