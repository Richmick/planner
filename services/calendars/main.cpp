#include <drogon/drogon.h>
#include <http_utils.hpp>
#include "scheduler/ruz_scheduler.hpp"

int main(int argc, const char* const* argv)
{
    if (argc > 2)
    {
        LOG_FATAL << "too many arguments (expected none or <config>)";
        return 1;
    }

    std::string config_file = "./config/calendars.json";
    if (argc > 1) config_file = argv[1];

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

    drogon::app().registerBeginningAdvice([]
    {
        api::calendars::start_ruz_scheduler();
    });
    LOG_INFO << "Calendars started. Listening...";
    drogon::app().run();
    LOG_INFO << "Calendars exited successfully";
}
