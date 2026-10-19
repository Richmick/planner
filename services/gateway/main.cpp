#include <drogon/drogon.h>
#include <http_utils.hpp>

int main(int argc, const char* const* argv)
{
	if (argc > 2)
	{
		LOG_FATAL << "too many arguments (expected none or <config>)";
		return 1;
	}
	std::string config_file = "./config/gateway.json";
	if (argc > 1)
	{
		config_file = argv[1];
	}
	try
	{
		drogon::app().loadConfigFile(config_file);
	}
	catch (const std::exception& e)
	{
		LOG_FATAL << "failed to load config \"" << config_file << "\": " << e.what();
		return 2;
	}
	//drogon::app().setExceptionHandler();
	drogon::app().run();
}
