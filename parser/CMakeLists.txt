file(GLOB parser_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(parser STATIC ${parser_sources})

target_include_directories(parser PUBLIC include)
target_link_libraries(parser PUBLIC ast)

if(BUILD_TESTING)
    file(GLOB parser_test_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )

    rls_add_gtest(parser_tests ${parser_test_sources})
    target_link_libraries(parser_tests PRIVATE parser)
endif()
