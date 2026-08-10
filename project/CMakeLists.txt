FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(project STATIC
    src/project.cpp
)

target_include_directories(project PUBLIC include)
target_link_libraries(project PUBLIC nlohmann_json::nlohmann_json)

if(BUILD_TESTING)
    rls_add_gtest(project_tests
        tests/project_tests.cpp
    )
    target_link_libraries(project_tests PRIVATE project)
endif()