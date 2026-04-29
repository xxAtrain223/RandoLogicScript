file(GLOB lsp_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(rls_lsp STATIC ${lsp_sources})

target_include_directories(rls_lsp PUBLIC include)
target_link_libraries(rls_lsp PUBLIC ast parser sema)

add_executable(rls_language_server
    main.cpp
)

target_link_libraries(rls_language_server PRIVATE rls_lsp)

if(BUILD_TESTING)
    file(GLOB lsp_test_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )

    rls_add_gtest(lsp_tests ${lsp_test_sources})
    target_link_libraries(lsp_tests PRIVATE rls_lsp)
    target_include_directories(lsp_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()
