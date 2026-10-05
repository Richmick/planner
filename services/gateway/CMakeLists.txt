cmake_minimum_required(VERSION 3.10)

project("planner-gateway"
	DESCRIPTION "Reverse proxy service for Planner appplication"
	VERSION 0.0.1
	LANGUAGES CXX
)

find_package(Drogon CONFIG REQUIRED)

# Parameters

if (NOT DEFINED PLANNER_CXX_STANDARD)
	if (NOT DEFINED CMAKE_CXX_STANDARD)
		set(PLANNER_GATEWAY_CXX_STANDARD 20)
	elseif (${CMAKE_CXX_STANDARD} LESS 20)
		message(WARNING "C++20 required - use it over cmake global C++${CMAKE_CXX_STANDARD}")
		set(PLANNER_GATEWAY_CXX_STANDARD 20)
	else()
		set(PLANNER_GATEWAY_CXX_STANDARD ${CMAKE_CXX_STANDARD})
	endif()
else()
	set(PLANNER_GATEWAY_CXX_STANDARD ${PLANNER_CXX_STANDARD})
endif()
if (PLANNER_GATEWAY_CXX_STANDARD LESS 20)
	message(FATAL_ERROR "Gateway: at least C++20 is required")
else()
	message(STATUS "Gateway: use C++${PLANNER_GATEWAY_CXX_STANDARD}")
endif()

if (NOT DEFINED PLANNER_RUNTIME_OUTPUT_DIRECTORY)
	set(PLANNER_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/service-executables)
endif()
if (NOT DEFINED PLANNER_CONFIG_DIRECTORY)
	set(PLANNER_CONFIG_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../config)
endif()

# Executable

add_executable("${PROJECT_NAME}")
set_target_properties("${PROJECT_NAME}" PROPERTIES
	CXX_STANDARD ${PLANNER_GATEWAY_CXX_STANDARD}
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/service-executables/
	RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/service-executables/
)
target_include_directories("${PROJECT_NAME}" PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/../"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../common"
)
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon)
target_sources("${PROJECT_NAME}" PRIVATE
	"main.cpp"
)

# Dependencies copying

add_custom_command(TARGET "${PROJECT_NAME}" POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
		${PLANNER_CONFIG_DIRECTORY}/gateway.json
		${PLANNER_RUNTIME_OUTPUT_DIRECTORY}/config/gateway.json
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_CURRENT_SOURCE_DIR}/../../.ssl/
		${PLANNER_RUNTIME_OUTPUT_DIRECTORY}/ssl/
)
