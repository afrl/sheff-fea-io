add_executable(feaio_dev source/main.cpp)
target_link_libraries(feaio_dev PRIVATE FeaIo::FeaIo)

include(${CMAKE_SOURCE_DIR}/cmake/Utilities.cmake)
assign_source_group("fea_io" ${FEAIO_HEADER_LIST})
