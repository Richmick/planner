cmake_minimum_required(VERSION 3.10)

project("planner-calendars"
    DESCRIPTION "Calendar data service for Planner application"
    VERSION 0.2.0
    LANGUAGES CXX
)

find_package(Drogon CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)

string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" is_top_level_project)
if (is_top_level_project)
    add_subdirectory("../../common/")
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
    include(set_language)
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
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/service-executables/"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/service-executables/"
)
target_include_directories("${PROJECT_NAME}" PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries("${PROJECT_NAME}" PRIVATE
    Drogon::Drogon
    PostgreSQL::PostgreSQL
    plannerlib
)
target_sources("${PROJECT_NAME}" PRIVATE
    "main.cpp"
    "controllers/calendars_ctrl.cc"
    "controllers/calendars_ctrl.h"
    "scheduler/ruz_scheduler.cc"
    "scheduler/ruz_scheduler.hpp"
)

add_custom_command(TARGET "${PROJECT_NAME}" POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PLANNER_RUNTIME_OUTPUT_DIRECTORY}/config"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PLANNER_RUNTIME_OUTPUT_DIRECTORY}/sql"
    COMMAND "${CMAKE_COMMAND}" -E copy
        "${PLANNER_CONFIG_DIRECTORY}/calendars.json"
        "${PLANNER_RUNTIME_OUTPUT_DIRECTORY}/config/calendars.json"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/sql/"
        "${PLANNER_RUNTIME_OUTPUT_DIRECTORY}/sql/"
)
