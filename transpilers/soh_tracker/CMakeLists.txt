file(GLOB soh_tracker_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(soh_tracker STATIC ${soh_tracker_sources})

target_include_directories(soh_tracker PUBLIC include)
target_link_libraries(soh_tracker PUBLIC ast)

if(BUILD_TESTING)
    file(GLOB soh_tracker_test_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )

    rls_add_gtest(soh_tracker_tests ${soh_tracker_test_sources})
    target_link_libraries(soh_tracker_tests PRIVATE soh_tracker sema)
    target_include_directories(soh_tracker_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()
