#pragma once

#include <memory>
#include <string>

namespace api
{
	void hash_password(const std::shared_ptr< std::string >& password);
}
