#include "users.hpp"

#include <sodium.h>

std::optional< std::string > api::users::hash_password(const std::string& password)
{
	char hashed[crypto_pwhash_STRBYTES];
	if (crypto_pwhash_str(hashed, password.c_str(), password.length(),
				crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
	{
		return std::nullopt;
	}
	return hashed;
}
