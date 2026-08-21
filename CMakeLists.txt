cmake_minimum_required(VERSION 3.14)

if(POLICY CMP0091)
	cmake_policy(SET CMP0091 NEW)
endif()

project(RandoLogicScript)
set(CMAKE_CXX_STANDARD 20)

option(RLS_STATIC_MSVC_RUNTIME "Link the MSVC runtime statically" OFF)
if(MSVC AND RLS_STATIC_MSVC_RUNTIME)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(CTest)
include(FetchContent)

if(BUILD_TESTING)
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

	FetchContent_Declare(
		googletest
		GIT_REPOSITORY https://github.com/google/googletest.git
		GIT_TAG v1.17.0
	)
	FetchContent_MakeAvailable(googletest)

	include(GoogleTest)

	function(rls_add_gtest TARGET_NAME)
		add_executable(${TARGET_NAME} ${ARGN})
		target_link_libraries(${TARGET_NAME} PRIVATE GTest::gtest_main)
		gtest_discover_tests(${TARGET_NAME})
	endfunction()
endif()

add_subdirectory(ast)
add_subdirectory(parser)
add_subdirectory(sema)
add_subdirectory(transpilers)
add_subdirectory(project)
add_subdirectory(console)
add_subdirectory(lsp)