file(GLOB sema_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(sema STATIC ${sema_sources})

target_include_directories(sema PUBLIC include)
target_link_libraries(sema PUBLIC ast)

if(BUILD_TESTING)
    file(GLOB sema_test_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )

    rls_add_gtest(sema_tests ${sema_test_sources})
    target_link_libraries(sema_tests PRIVATE sema parser)
    target_include_directories(sema_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()
