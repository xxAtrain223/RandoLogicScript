add_executable(RandoLogicScript
    main.cpp
)

target_link_libraries(RandoLogicScript PRIVATE ast parser sema soh ap)

if(BUILD_TESTING)
    rls_add_gtest(console_acceptance_tests
        tests/acceptance_soh_tests.cpp
        tests/acceptance_ap_tests.cpp
    )
    target_link_libraries(console_acceptance_tests PRIVATE ast parser sema soh ap)
    target_compile_definitions(console_acceptance_tests PRIVATE
        RLS_REPO_ROOT="${CMAKE_SOURCE_DIR}"
    )
endif()
