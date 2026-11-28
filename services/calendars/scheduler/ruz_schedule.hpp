#pragma once

#include <chrono>
#include <string>
#include <vector>

#include <json/json.h>

namespace api::calendars::ruz
{
	struct date_range
	{
		std::chrono::local_days from;
		std::chrono::local_days to;
		friend auto operator<=>(const date_range&, const date_range&) = default;
	};

	struct schedule_entry
	{
		std::string external_id;
		std::string name;
		std::chrono::utc_seconds start;
		std::chrono::utc_seconds end;
		Json::Value source;
	};

	std::string format_date(std::chrono::local_days date);
	std::string format_utc(std::chrono::utc_seconds time);
	std::vector< schedule_entry > read_schedule(const Json::Value& payload,
			const std::chrono::time_zone& zone, date_range range);
	std::string legacy_external_id(const Json::Value& source);
}
