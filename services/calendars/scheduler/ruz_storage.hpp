#pragma once

#include <cstdint>

#include <drogon/drogon.h>

#include "ruz_schedule.hpp"

namespace api::calendars::ruz
{
	struct calendar
	{
		std::int64_t id;
		std::int64_t group_id;
		friend auto operator<=>(const calendar&, const calendar&) = default;
	};

	drogon::Task< std::vector< calendar > > load_calendars();
	drogon::Task<> save_schedule(calendar target, date_range range, const std::vector< schedule_entry >& entries);
}
