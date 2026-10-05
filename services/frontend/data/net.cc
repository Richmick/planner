#include "net.h"

innerplane::service innerplane::calendar;
innerplane::service innerplane::users;

void innerplane::init_services()
{
	Json::Value config = drogon::app().getCustomConfig();
	if (!config.isMember("net"))
	{
		LOG_ERROR << "no service network configuration";
		throw std::runtime_error("no network configuration");
	}
	Json::Value net = config.get("net", {});
	if (!net.isMember("calendar") || !net.isMember("users"))
	{
		LOG_ERROR << "no configuration for obligatory services";
		throw std::runtime_error("missing obligatory service configs");
	}
	calendar.init(net.get("calendar", {}));
	users.init(net.get("users", {}));
}
