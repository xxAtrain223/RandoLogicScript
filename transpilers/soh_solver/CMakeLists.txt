file(GLOB soh_solver_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(soh_solver STATIC ${soh_solver_sources})

target_include_directories(soh_solver PUBLIC include)
target_link_libraries(soh_solver PUBLIC ast)

if(BUILD_TESTING)
    file(GLOB soh_solver_test_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )

    rls_add_gtest(soh_solver_tests ${soh_solver_test_sources})
    target_link_libraries(soh_solver_tests PRIVATE soh_solver)
endif()
