#pragma once

#include <cstddef>
#include <string>
#include <chrono>
#include <vector>

namespace data
{
	struct user_t
	{
		using timepoint_t = std::chrono::sys_time< std::chrono::seconds >;
		std::size_t id;
		std::string login;
		timepoint_t last_login;
		std::vector< std::size_t > subscribes;
		bool is_admin = false;
	};
	bool is_email(const std::string& str);
	bool is_login(const std::string& str);
}
