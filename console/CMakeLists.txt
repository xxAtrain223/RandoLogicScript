add_executable(RandoLogicScript
    main.cpp
)

target_link_libraries(RandoLogicScript PRIVATE ast parser soh_solver)
