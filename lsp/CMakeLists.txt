FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)
find_package(Threads REQUIRED)

file(GLOB lsp_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(rls_lsp STATIC ${lsp_sources})

target_include_directories(rls_lsp PUBLIC include)
target_link_libraries(rls_lsp PUBLIC
    nlohmann_json::nlohmann_json
    project
    sema
    Threads::Threads
)

add_executable(rls_language_server
    main.cpp
)
target_link_libraries(rls_language_server PRIVATE rls_lsp)

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    file(GLOB lsp_test_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )

    rls_add_gtest(lsp_tests ${lsp_test_sources})
    target_link_libraries(lsp_tests PRIVATE rls_lsp)

    add_test(
        NAME lsp_process_smoke
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/process_smoke.py"
            --server "$<TARGET_FILE:rls_language_server>"
    )
endif()