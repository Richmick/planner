#pragma once

namespace api::calendars
{
	inline constexpr char subscribed_calendar_read_access[] =
			" AND (c.visibility = 'public' OR c.owner_id = s.user_id"
			" OR (c.visibility = 'protected' AND EXISTS ("
			" SELECT 1 FROM calendar_access a WHERE a.calendar_id = c.id AND a.user_id = s.user_id)))";
}
