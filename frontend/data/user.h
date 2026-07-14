#pragma once

#include <cstddef>
#include <string>
#include <chrono>
#include <sodium.h>

namespace data
{
	struct user_t
	{
		using timepoint_t = std::chrono::sys_time< std::chrono::seconds >;
		bool exist = false;
		std::size_t id;
		std::string login;
		timepoint_t last_login;
		char encrypted_password[crypto_pwhash_STRBYTES];
	};
	bool is_email(const std::string& str);
	bool is_login(const std::string& str);
	void screen(unsigned char* res, const unsigned char* str, std::size_t str_len);
	bool descreen(unsigned char* res, const unsigned char* enc, std::size_t enc_len);
}
