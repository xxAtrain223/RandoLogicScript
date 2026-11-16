file(GLOB soh_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# Embed rls_match.h content into a generated header so the transpiler can
# output it without maintaining a duplicate copy.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/rls_match.h" RLS_MATCH_CONTENT)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/rls_match.h")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rls_match_content.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/rls_match_content.h"
    @ONLY
)

add_library(soh STATIC ${soh_sources})

target_include_directories(soh PUBLIC include)
target_include_directories(soh PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(soh PUBLIC ast)
target_link_libraries(soh PRIVATE rls_soh_host_mappings)

if(BUILD_TESTING)
    file(GLOB soh_test_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )

    rls_add_gtest(soh_tests ${soh_test_sources})
    target_link_libraries(soh_tests PRIVATE soh sema)
    target_include_directories(soh_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

    add_executable(soh_match_fixture_generator
        tests/match_execution/generate_fixtures.cpp)
    target_link_libraries(soh_match_fixture_generator PRIVATE soh sema)

    set(match_fixture_header
        "${CMAKE_CURRENT_BINARY_DIR}/generated/match_execution_fixtures.h")
    add_custom_command(
        OUTPUT "${match_fixture_header}"
        COMMAND soh_match_fixture_generator
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/match_execution/fixtures.rls"
            "${match_fixture_header}"
        DEPENDS soh_match_fixture_generator
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/match_execution/fixtures.rls"
        VERBATIM
    )
    target_sources(soh_tests PRIVATE "${match_fixture_header}")
    target_include_directories(soh_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
endif()
