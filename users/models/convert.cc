#include "convert.h"

#include <stdexcept>
#include <sodium.h>

void api::hash_password(const std::shared_ptr< std::string >& password)
{
	if (!password)
	{
		throw std::runtime_error("no password");
	}
	char hashed[crypto_pwhash_STRBYTES];
	if (crypto_pwhash_str(hashed, password->c_str(), password->length(),
				crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
	{
		throw std::runtime_error("failed to hash password");
	}
	*password = hashed;
}
