#include <drogon/drogon.h>
#include <http_utils.hpp>

int main(int argc, const char* const* argv)
{
	if (argc > 2)
	{
		LOG_FATAL << "too many arguments (expected none or <config>)";
		return 1;
	}
	std::string config_file = "./config/scheduler.json";
	if (argc > 1)
	{
		config_file = argv[1];
	}
	try
	{
		drogon::app().loadConfigFile(config_file);
		drogon::app().setCustom404Page(api::response::new_404_not_found());
	}
	catch (const std::exception& e)
	{
		LOG_FATAL << "failed to load config \"" << config_file << "\": " << e.what();
		return 2;
	}
	LOG_INFO << "Scheduler started. Listening...";
	drogon::app().run();
	LOG_INFO << "Scheduler exited successfully";
}
