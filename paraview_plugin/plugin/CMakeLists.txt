cmake_policy(SET CMP0079 NEW)

paraview_add_plugin(FEAIOReader
   VERSION "1.0" 
   MODULES FEAIOReaders
   MODULE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/FEAIOReader/vtk.module")

target_include_directories(FEAIOReaders PUBLIC FEAIOReader)
target_link_libraries(FEAIOReaders PUBLIC Eigen3::Eigen)
target_link_libraries(FEAIOReaders PRIVATE FeaIo::FeaIo)