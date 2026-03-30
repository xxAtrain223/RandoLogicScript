add_library(ast INTERFACE)
target_include_directories(ast INTERFACE include)

if(BUILD_TESTING)
	file(GLOB ast_test_sources CONFIGURE_DEPENDS
		"${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
	)

	rls_add_gtest(ast_tests ${ast_test_sources})
	target_link_libraries(ast_tests PRIVATE ast)
endif()