add_executable(RandoLogicScript
    main.cpp
)

target_link_libraries(RandoLogicScript PRIVATE ast sema soh ap project)

if(BUILD_TESTING)
    rls_add_gtest(console_acceptance_tests
        tests/acceptance_soh_tests.cpp
        tests/acceptance_ap_tests.cpp
    )
    target_link_libraries(console_acceptance_tests PRIVATE ast sema soh ap)
    target_compile_definitions(console_acceptance_tests PRIVATE
        RLS_REPO_ROOT="${CMAKE_SOURCE_DIR}"
    )

    rls_add_gtest(console_project_tests
        tests/cli_project_tests.cpp
    )
    add_dependencies(console_project_tests RandoLogicScript)
    target_compile_definitions(console_project_tests PRIVATE
        RLS_CONSOLE_PATH="$<TARGET_FILE:RandoLogicScript>"
    )
endif()
