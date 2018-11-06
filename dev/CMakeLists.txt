set(FEA_IO_DEV_TARGET_NAME ${PROJECT_NAME}_dev)
add_executable(${FEA_IO_DEV_TARGET_NAME} src/main.cpp)
target_link_libraries(${FEA_IO_DEV_TARGET_NAME} PRIVATE ${FEA_IO_TARGET_NAME})
assign_source_group(${FEA_IO_HEADER_LIST})

# Configure MPI if it was found
if(MPI_FOUND)
    target_compile_definitions(${FEA_IO_DEV_TARGET_NAME} PUBLIC HAVE_MPI)

    target_link_libraries(${FEA_IO_DEV_TARGET_NAME} PRIVATE ${MPI_LIBRARIES})

    if(MPI_COMPILE_FLAGS)
    set_target_properties(${FEA_IO_DEV_TARGET_NAME} PROPERTIES
        COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
    endif()

    if(MPI_LINK_FLAGS)
    set_target_properties(${FEA_IO_DEV_TARGET_NAME} PROPERTIES
        LINK_FLAGS "${MPI_LINK_FLAGS}")
    endif()
endif()

# Configure JSON
target_link_libraries(${FEA_IO_DEV_TARGET_NAME} PRIVATE nlohmann_json::nlohmann_json)