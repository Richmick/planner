#include "calendars_ctrl.hpp"

#include "database_utils.hpp"

#include <algorithm>
#include <chrono>
#include <optional>
#include <sstream>

namespace api::calendars
{
	namespace
	{
		struct event_period
		{
			std::int64_t start;
			std::int64_t end;
		};

		std::optional< std::chrono::local_days > parse_date(std::string_view date)
		{
			if (date.size() != 10 || date[4] != '-' || date[7] != '-') return std::nullopt;
			const auto is_digit = [](char c) { return '0' <= c && c <= '9'; };
			if (!std::ranges::all_of(date.substr(0, 4), is_digit)
						|| !std::ranges::all_of(date.substr(5, 2), is_digit)
						|| !std::ranges::all_of(date.substr(8, 2), is_digit))
				return std::nullopt;
			std::istringstream input{std::string(date)};
			std::chrono::year_month_day parsed;
			std::chrono::from_stream(input, "%F", parsed);
			if (input.fail() || !parsed.ok()) return std::nullopt;
			return std::chrono::local_days(parsed);
		}

		std::string event_query(bool active_only)
		{
			std::string sql =
					"WITH visible_events AS ("
					" SELECT e.id AS event_id, e.calendar_id, c.type_id AS source_type, e.name,"
					" COALESCE(o.local_scheduled, e.global_scheduled) AS effective_scheduled,"
					" o.confirmed, COALESCE(o.finished, e.finished, FALSE) AS is_finished,"
					" COALESCE(o.success, FALSE) AS is_success,"
					" COALESCE(o.dependencies, e.base_dependencies) AS effective_dependencies,"
					" COALESCE(o.restrictions, e.base_restrictions) AS effective_restrictions,"
					" e.overlap, e.interruptible, e.version"
					" FROM events e JOIN calendars c ON c.id = e.calendar_id"
					" JOIN subscriptions s ON s.calendar_id = e.calendar_id"
					" LEFT JOIN event_overrides o ON o.event_id = e.id AND o.user_id = s.user_id"
					" WHERE s.user_id = ?"
					") SELECT * FROM visible_events";
			const bool postgres = db::is_postgresql();
			if (active_only)
			{
				sql += " WHERE is_finished = FALSE AND ";
				sql += postgres ? "effective_scheduled <> '[]'::jsonb" : "json(effective_scheduled) <> '[]'";
			}
			else
			{
				sql += postgres
						? " WHERE EXISTS (SELECT 1 FROM jsonb_array_elements(effective_scheduled) slot"
						  " WHERE (slot->>'start')::timestamptz < to_timestamp(CAST(? AS BIGINT))"
						  " AND (slot->>'end')::timestamptz >= to_timestamp(CAST(? AS BIGINT)))"
						: " WHERE EXISTS (SELECT 1 FROM json_each(effective_scheduled) slot"
						  " WHERE unixepoch(slot.value ->> 'start') < ?"
						  " AND unixepoch(slot.value ->> 'end') >= ?)";
			}
			sql += postgres
					? " ORDER BY (effective_scheduled->0->>'start')::timestamptz NULLS LAST, event_id"
					: " ORDER BY unixepoch(effective_scheduled ->> '$[0].start') NULLS LAST, event_id";
			return db::query(sql);
		}

		event_period utc_period(std::chrono::local_days from, std::chrono::local_days to)
		{
			const auto* zone = std::chrono::locate_zone("Europe/Moscow");
			const auto start = zone->to_sys(from, std::chrono::choose::earliest);
			const auto end = zone->to_sys(to + std::chrono::days(1), std::chrono::choose::earliest);
			return {start.time_since_epoch().count(), end.time_since_epoch().count()};
		}
	}

	api::response_t calendars_ctrl::events_response(const drogon::orm::Result& rows)
	{
		Json::Value events(Json::arrayValue);
		for (const auto& row: rows)
		{
			Json::Value event;
			event["event_id"] = row["event_id"].as< std::int64_t >();
			event["calendar_id"] = row["calendar_id"].as< std::int64_t >();
			event["source_type"] = row["source_type"].as< std::string >();
			event["name"] = row["name"].as< std::string >();
			event["scheduled"] = field_json(row, "effective_scheduled");
			event["confirmed"] = field_json(row, "confirmed");
			event["finished"] = row["is_finished"].as< bool >();
			event["success"] = row["is_success"].as< bool >();
			event["dependencies"] = field_json(row, "effective_dependencies");
			event["restrictions"] = field_json(row, "effective_restrictions");
			event["overlap"] = row["overlap"].as< std::string >();
			event["interruptible"] = row["interruptible"].as< bool >();
			event["version"] = row["version"].as< int >();
			events.append(std::move(event));
		}
		return api::response::new_json(std::move(events));
	}

	drogon::Task<> calendars_ctrl::get_events(api::request_t request, api::response_callback callback)
	{
		const std::int64_t user_id = parse_id(request->getHeader("user-id"));
		if (user_id == -1) co_return callback(resp.k401_unauthorized);
		const std::string& from = request->getParameter("from");
		const std::string& to = request->getParameter("to");
		const auto first = parse_date(from.empty() ? request->getParameter("date_from") : from);
		const auto last = parse_date(to.empty() ? request->getParameter("date_to") : to);
		if (!first || !last || *first > *last) co_return callback(resp.k400_invalid_dates);

		try
		{
			const event_period range = utc_period(*first, *last);
			const auto rows = co_await drogon::app().getDbClient()->execSqlCoro(
					event_query(false), user_id, range.end, range.start);
			co_return callback(events_response(rows));
		}
		catch (const drogon::orm::DrogonDbException& error)
		{
			co_return callback(database_error(error));
		}
		catch (const std::exception& error)
		{
			co_return callback(database_error(error));
		}
	}

	drogon::Task<> calendars_ctrl::get_active_events(api::request_t request, api::response_callback callback)
	{
		const std::int64_t user_id = parse_id(request->getHeader("user-id"));
		if (user_id == -1) co_return callback(resp.k401_unauthorized);
		try
		{
			const auto rows = co_await drogon::app().getDbClient()->execSqlCoro(event_query(true), user_id);
			co_return callback(events_response(rows));
		}
		catch (const drogon::orm::DrogonDbException& error)
		{
			co_return callback(database_error(error));
		}
	}
}
