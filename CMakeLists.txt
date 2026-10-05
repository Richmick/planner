cmake_minimum_required(VERSION 3.10)

project(planner
	DESCRIPTION "Planner microservice web application"
	VERSION 0.1.0
	LANGUAGES CXX
)

# Parameters

set(PLANNER_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/service-executables)
set(PLANNER_CONFIG_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/config)

if (NOT DEFINED CMAKE_CXX_STANDARD)
	set(PLANNER_CXX_STANDARD 20)
elseif (${CMAKE_CXX_STANDARD} LESS 20)
	message(WARNING "C++20 required - use it over cmake global C++${CMAKE_CXX_STANDARD}")
	set(PLANNER_CXX_STANDARD 20)
else()
	set(PLANNER_CXX_STANDARD ${CMAKE_CXX_STANDARD})
endif()

# Targets

#add_subdirectory(external/os-tools)

add_subdirectory(services/gateway)
#add_subdirectory(services/frontend)
#add_subdirectory(services/users)
#add_subdirectory(services/calendar)

add_custom_target("build-all" DEPENDS
	planner-gateway
	#planner-frontend
	#planner-users
	#planner-calendar
)
