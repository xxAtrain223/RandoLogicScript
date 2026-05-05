file(GLOB ap_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(ap STATIC ${ap_sources})

target_include_directories(ap PUBLIC include)
target_link_libraries(ap PUBLIC ast)

if(BUILD_TESTING)
    file(GLOB ap_test_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )

    rls_add_gtest(ap_tests ${ap_test_sources})
    target_link_libraries(ap_tests PRIVATE ap)
endif()
