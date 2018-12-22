add_executable(feaio_dev src/main.cpp)
target_link_libraries(feaio_dev PRIVATE FEAIO::FEAIO)

include(${CMAKE_SOURCE_DIR}/cmake/Utilities.cmake)
assign_source_group("fea_io" ${FEAIO_HEADER_LIST})
