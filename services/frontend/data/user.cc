#include "user.h"

#include <cctype>

bool data::is_email(const std::string& str)
{
	bool has_domain = false;
	for (char c: str)
	{
		if (c == '@')
		{
			if (has_domain) return false;
			has_domain = true;
			continue;
		}
		if (!std::isalnum(static_cast< unsigned char >(c)) && (c != '_') && (c != '.'))
		{
			return false;
		}
	}
	return has_domain;
}
bool data::is_login(const std::string& str)
{
	for (char c: str)
	{
		if (!std::isalnum(static_cast< unsigned char >(c)) && (c != '_'))
		{
			return false;
		}
	}
	return true;
}
namespace data
{
	static unsigned char screen(unsigned char bin)
	{
		if (bin < 10) return bin + '0';
		if (bin < 16) return bin - 10 + 'A';
		return 0xFF;
	}
	static unsigned char descreen(unsigned char enc)
	{
		if ((enc >= '0') && (enc <= '9')) return enc - '0';
		if ((enc >= 'A') && (enc <= 'F')) return enc - 'A' + 10;
		return 0xFF;
	}
}
