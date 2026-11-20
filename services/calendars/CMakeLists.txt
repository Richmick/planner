cmake_minimum_required(VERSION 3.10)

project("planner-calendars"
	DESCRIPTION "Calendar data service for Planner application"
	VERSION 0.2.0
	LANGUAGES CXX
)

find_package(Drogon CONFIG REQUIRED)
if (pg_FOUND)
	find_package(PostgreSQL QUIET)
endif()

string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" is_top_level_project)
if (is_top_level_project)
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
	include(set_language)
	add_subdirectory("../../common/" "${CMAKE_CURRENT_BINARY_DIR}/common")
endif()

setup_cpp_std(Calendars 20)

if (NOT DEFINED PLANNER_RUNTIME_OUTPUT_DIRECTORY)
	set(PLANNER_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/service-executables")
endif()
if (NOT DEFINED PLANNER_CONFIG_DIRECTORY)
	set(PLANNER_CONFIG_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../config")
endif()

add_executable("${PROJECT_NAME}")
set_target_properties("${PROJECT_NAME}" PROPERTIES
	CXX_STANDARD ${PLANNER_CALENDARS_CXX_STANDARD}
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
	RUNTIME_OUTPUT_DIRECTORY "${PLANNER_RUNTIME_OUTPUT_DIRECTORY}"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${PLANNER_RUNTIME_OUTPUT_DIRECTORY}"
)
target_include_directories("${PROJECT_NAME}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries("${PROJECT_NAME}" PRIVATE Drogon::Drogon plannerlib)
if (pg_FOUND AND TARGET PostgreSQL::PostgreSQL)
	target_link_libraries("${PROJECT_NAME}" PRIVATE PostgreSQL::PostgreSQL)
endif()
target_sources("${PROJECT_NAME}" PRIVATE
	"main.cpp"
	"controllers/calendars_ctrl.cpp"
	"controllers/calendar_sources.cpp"
	"controllers/calendar_events.cpp"
	"controllers/controller_utils.cpp"
	"scheduler/ruz_scheduler.cpp"
	"scheduler/ruz_client.cpp"
	"scheduler/ruz_schedule.cpp"
	"scheduler/ruz_storage.cpp"
	"outbox/outbox_relay.cpp"
)

add_custom_command(TARGET "${PROJECT_NAME}" POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${PLANNER_RUNTIME_OUTPUT_DIRECTORY}/config"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${PLANNER_CONFIG_DIRECTORY}/calendars.json"
		"${PLANNER_RUNTIME_OUTPUT_DIRECTORY}/config/calendars.json"
	COMMAND "${CMAKE_COMMAND}"
		"-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/calendars.sqlite"
		"-DDESTINATION=${PLANNER_RUNTIME_OUTPUT_DIRECTORY}/db/calendars.sqlite"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/copy_if_missing.cmake"
	VERBATIM
)
