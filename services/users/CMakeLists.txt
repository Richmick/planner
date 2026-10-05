cmake_minimum_required(VERSION 3.5)
project(planner-users
	DESCRIPTION "Service with user profiles"
	VERSION 0.1.0
	LANGUAGES CXX
)

find_package(Drogon CONFIG REQUIRED)
find_package(libsodium CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(PLANNER_USERS_CXX_STANDARD, ${CMAKE_CXX_STANDARD})

if (PLANNER_USERS_CXX_STANDARD LESS 20)
	message(STATUS "use c++17")
else ()
	message(STATUS "use c++${PLANNER_USERS_CXX_STANDARD}")
endif ()

add_executable("${PROJECT_NAME}")
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon libsodium::libsodium)
set_target_properties(${TARGET} PROPERTIES
	CXX_STANDARD ${PLANNER_USERS_CXX_STANDARD}
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
)
target_include_directories(${PROJECT_NAME}
			PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
				${CMAKE_CURRENT_SOURCE_DIR}/models ${CMAKE_CURRENT_SOURCE_DIR}/../common)

aux_source_directory(controllers CTRL_SRC)
aux_source_directory(models MODEL_SRC)
aux_source_directory(../../common COMMON_SRC)
target_sources("${PROJECT_NAME}" PRIVATE
	"main.cc"
	${CTRL_SRC}
	${MODEL_SRC}
	${COMMON_SRC}
)

# Copy database
if ((NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/users.sqlite)
		OR (${CMAKE_CURRENT_SOURCE_DIR}/users.sqlite IS_NEWER_THAN ${CMAKE_CURRENT_BINARY_DIR}/users.sqlite))
	file(COPY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/users.sqlite ${CMAKE_CURRENT_BINARY_DIR}/users.sqlite)
endif()
