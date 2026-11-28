#include <drogon/drogon.h>
#include <http_utils.hpp>

int main(int argc, const char* const* argv)
{
	if (argc > 2)
	{
		LOG_FATAL << "too many arguments (expected none or <config>)";
		return 1;
	}

	const std::string config_file = argc > 1 ? argv[1] : "./config/calendars.json";
	try
	{
		drogon::app().loadConfigFile(config_file);
		drogon::app().setCustom404Page(api::response::new_404_not_found());
	}
	catch (const std::exception& error)
	{
		LOG_FATAL << "failed to load config \"" << config_file << "\": " << error.what();
		return 2;
	}

	LOG_INFO << "Calendars started. Listening...";
	try
	{
		drogon::app().run();
	}
	catch (const std::exception& error)
	{
		LOG_FATAL << "Calendars failed: " << error.what();
		return 3;
	}
	LOG_INFO << "Calendars exited successfully";
}
