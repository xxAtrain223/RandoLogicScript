file(GLOB ap_transpiler_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(ap_transpiler STATIC ${ap_transpiler_sources})

target_include_directories(ap_transpiler PUBLIC include)
target_link_libraries(ap_transpiler PUBLIC ast)

if(BUILD_TESTING)
    file(GLOB ap_transpiler_test_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )

    rls_add_gtest(ap_transpiler_tests ${ap_transpiler_test_sources})
    target_link_libraries(ap_transpiler_tests PRIVATE ap_transpiler parser sema)
endif()
