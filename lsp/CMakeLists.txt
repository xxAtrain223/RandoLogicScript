FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

file(GLOB lsp_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/endpoints/*.cpp"
)

file(GLOB lsp_endpoint_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/endpoints/*.cpp"
)

add_library(rls_lsp STATIC ${lsp_sources})

target_include_directories(rls_lsp
    PUBLIC include
    PRIVATE src)
target_link_libraries(rls_lsp PUBLIC ast parser sema nlohmann_json::nlohmann_json)

set_source_files_properties(${lsp_endpoint_sources} PROPERTIES COMPILE_OPTIONS
    "$<$<CXX_COMPILER_ID:MSVC>:/wd4189>"
    "$<$<CXX_COMPILER_ID:GNU>:-Wno-unused-variable;-Wno-unused-const-variable>"
    "$<$<CXX_COMPILER_ID:Clang>:-Wno-unused-variable;-Wno-unused-const-variable>")

function(rls_link_lsp TARGET_NAME)
    target_link_libraries(${TARGET_NAME} PRIVATE rls_lsp)

    if(MSVC)
        target_link_options(${TARGET_NAME} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:rls_lsp>")
    elseif(APPLE)
        target_link_options(${TARGET_NAME} PRIVATE "LINKER:-force_load,$<TARGET_FILE:rls_lsp>")
    else()
        target_link_options(${TARGET_NAME} PRIVATE
            "LINKER:--whole-archive"
            "$<TARGET_FILE:rls_lsp>"
            "LINKER:--no-whole-archive")
    endif()
endfunction()

add_executable(rls_language_server
    main.cpp
)

rls_link_lsp(rls_language_server)

if(BUILD_TESTING)
    file(GLOB lsp_test_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )

    rls_add_gtest(lsp_tests ${lsp_test_sources})
    rls_link_lsp(lsp_tests)
    target_include_directories(lsp_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()
