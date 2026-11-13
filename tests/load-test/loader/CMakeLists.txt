cmake_minimum_required(VERSION 3.10)

project("planner-load-test-loader"
	DESCRIPTION "DDoS client"
	VERSION 0.0.1
	LANGUAGES CXX
)

find_package(Drogon CONFIG REQUIRED)
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" is_top_level_project)
if (is_top_level_project)
	add_subdirectory("../../../common/")
endif()

# Parameters

setup_cpp_std(Loader 23)
if (NOT DEFINED PLANNER_RUNTIME_OUTPUT_DIRECTORY)
	set(PLANNER_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/service-executables")
endif()
if (NOT DEFINED PLANNER_CONFIG_DIRECTORY)
	set(PLANNER_CONFIG_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../../config")
endif()

# Executable

add_executable("${PROJECT_NAME}")
set_target_properties("${PROJECT_NAME}" PROPERTIES
	CXX_STANDARD ${PLANNER_LOADER_CXX_STANDARD}
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/service-executables/"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/service-executables/"
)
target_include_directories("${PROJECT_NAME}" PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries("${PROJECT_NAME}" PRIVATE Drogon::Drogon)
target_sources("${PROJECT_NAME}" PRIVATE
	"main.cpp"
	"plugins/ddos.cpp"
	"plugins/ddos_config_parse.cpp"
	"plugins/ddos.hpp"
)

# Dependencies copying

add_custom_command(TARGET "${PROJECT_NAME}" POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E copy
		"${PLANNER_CONFIG_DIRECTORY}/load-test.json"
		"${PLANNER_RUNTIME_OUTPUT_DIRECTORY}/config/load-test.json"
)
