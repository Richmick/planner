#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace api::users
{
	using id_t = std::int64_t;
	std::optional< std::string > hash_password(const std::string& password);
}
