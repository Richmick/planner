#pragma once

#include <drogon/drogon.h>

namespace api::calendars::ruz
{
	drogon::Task< Json::Value > search_groups(std::string name);
	void sync_calendar(std::int64_t calendar_id, std::int64_t group_id);
}
