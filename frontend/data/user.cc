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

void data::screen(unsigned char*res, const unsigned char* str, std::size_t str_len)
{
	for (std::size_t i = 0; i < str_len; i++)
	{
		res[i << 1] = screen((str[i] >> 4) & 0xF);
		res[(i << 1) + 1] = screen(str[i] & 0xF);
	}
}
bool data::descreen(unsigned char* res, const unsigned char* enc, std::size_t enc_len)
{
	if (enc == nullptr) return false;

	if (enc_len & 0b1)
	{
		res[0] = descreen(enc[0]);
		++res, ++enc, --enc_len;
	}
	enc_len >>= 1;
	for (std::size_t i = 0; i < enc_len; i++)
	{
		unsigned char lhs = descreen(enc[i << 1]), rhs = descreen(enc[(i << 1) + 1]);
		if ((lhs == 0xFF) || (rhs == 0xFF)) return false;
		res[i] = (lhs << 4) | rhs;
	}
	return true;
}