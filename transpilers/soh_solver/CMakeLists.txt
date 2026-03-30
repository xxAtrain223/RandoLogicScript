add_library(soh_solver STATIC
    src/soh_solver.cpp
)

target_include_directories(soh_solver PUBLIC include)
target_link_libraries(soh_solver PUBLIC ast)
