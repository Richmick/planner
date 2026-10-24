cmake_minimum_required(VERSION 3.10)

find_package(Drogon CONFIG REQUIRED)

setup_cpp_std(Commonlib 23)

add_library(plannerlib OBJECT)
target_include_directories("plannerlib" PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}"
)
target_sources("plannerlib" PRIVATE
	"http_utils.cpp"
	"http_utils.hpp"
	"health_check.cpp"
	"health_check.hpp"
	"constants.hpp"
	"filters/account.cpp"
	"filters/account.hpp"
)
set_target_properties("plannerlib" PROPERTIES
	CXX_STANDARD ${PLANNER_COMMONLIB_CXX_STANDARD}
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
)
target_link_libraries("plannerlib" PRIVATE Drogon::Drogon)
