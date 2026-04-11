file(GLOB soh_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(soh STATIC ${soh_sources})

target_include_directories(soh PUBLIC include)
target_link_libraries(soh PUBLIC ast)

if(BUILD_TESTING)
    file(GLOB soh_test_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )

    rls_add_gtest(soh_tests ${soh_test_sources})
    target_link_libraries(soh_tests PRIVATE soh)
endif()
