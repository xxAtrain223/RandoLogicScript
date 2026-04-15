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

if(BUILD_TESTING)
    file(GLOB soh_test_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )

    rls_add_gtest(soh_tests ${soh_test_sources})
    target_link_libraries(soh_tests PRIVATE soh parser sema)
    target_include_directories(soh_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()
