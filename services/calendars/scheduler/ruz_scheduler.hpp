#pragma once

#include <cstdint>
#include <string>

namespace api::calendars
{
    void start_ruz_scheduler();
    void sync_ruz_calendar(std::int64_t calendar_id, std::string group_id);
}
