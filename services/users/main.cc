#include <drogon/drogon.h>
#include <sodium.h>

int main()
{
	if (sodium_init() < 0)
	{
		std::cerr << "failed to initialize crypto library\n";
		return 1;
	}
	drogon::app().loadConfigFile("../config.json");
	drogon::app().run();
	return 0;
}
