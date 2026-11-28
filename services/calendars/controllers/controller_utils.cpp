#include "calendars_ctrl.hpp"

#include <charconv>
#include <sstream>

namespace api::calendars
{
	calendars_ctrl::responses::responses():
		k204_access_granted(api::response::new_response(drogon::k204NoContent)),
		k400_invalid_body(api::response::new_400("INVALID_BODY", "Expected a JSON object")),
		k400_invalid_source_type(api::response::new_400("INVALID_SOURCE_TYPE",
				"Source type must contain 1 to 7 letters, digits, underscores or hyphens")),
		k400_invalid_description(api::response::new_400("INVALID_DESCRIPTION", "Description must be a string")),
		k400_invalid_name(api::response::new_400("INVALID_NAME", "Calendar name must be a nonempty string")),
		k400_invalid_external_id(api::response::new_400("INVALID_EXTERNAL_ID",
				"External calendar ID must be a nonempty string of at most 255 characters")),
		k400_invalid_dates(api::response::new_400("INVALID_DATES",
				"Supply dates in YYYY-MM-DD format with from on or before to")),
		k400_invalid_calendar_id(api::response::new_400("INVALID_CALENDAR_ID", "Calendar ID must be a positive integer")),
		k400_invalid_user_id(api::response::new_400("INVALID_USER_ID", "User ID must be a positive integer")),
		k400_invalid_access(api::response::new_400("INVALID_ACCESS", "Access must be r, w or s")),
		k400_invalid_access_event(api::response::new_400("INVALID_ACCESS_EVENT", "Expected an access-request event")),
		k401_unauthorized(api::response::new_401_unauthorized()),
		k403_forbidden(api::response::new_403_forbidden()),
		k404_not_found(api::response::new_404_not_found()),
		k409_access_already_granted(api::response::new_error(drogon::k409Conflict,
				"ACCESS_ALREADY_GRANTED", "You already have the requested access")),
		k500_database_error(api::response::new_error(drogon::k500InternalServerError,
				"DB_ERROR", "Internal database error")),
		k502_source_error(api::response::new_error(drogon::k502BadGateway,
				"SOURCE_UNAVAILABLE", "RUZ service is unavailable"))
	{}

	std::int64_t calendars_ctrl::parse_id(std::string_view value)
	{
		if (value.empty()) return -1;

		std::int64_t id = -1;
		const std::from_chars_result parsed = std::from_chars(value.data(), value.data() + value.size(), id);
		if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || id <= 0)
			return -1;
		return id;
	}

	std::int64_t calendars_ctrl::parse_json_id(const Json::Value& value)
	{
		if (value.isString()) return parse_id(value.asString());
		if ((value.type() != Json::intValue && value.type() != Json::uintValue) || !value.isInt64()) return -1;
		const std::int64_t id = value.asInt64();
		return id > 0 ? id : -1;
	}

	bool calendars_ctrl::valid_source_type(std::string_view value)
	{
		constexpr std::string_view characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
		return !value.empty() && value.size() <= 7 && value.find_first_not_of(characters) == std::string_view::npos;
	}

	Json::Value calendars_ctrl::field_json(const drogon::orm::Row& row, const char* name)
	{
		if (row[name].isNull()) return {};

		const std::string value = row[name].as< std::string >();
		std::istringstream input(value);
		Json::Value result;
		if (!Json::parseFromStream(Json::CharReaderBuilder{}, input, &result, nullptr)) return Json::Value(value);
		return result;
	}

	std::string_view calendars_ctrl::calendar_created_at_sql(bool postgresql)
	{
		return postgresql
				? "to_char(c.created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"')"
				: "strftime('%Y-%m-%dT%H:%M:%fZ', c.created_at)";
	}

	Json::Value calendars_ctrl::calendar_json(const drogon::orm::Row& row)
	{
		Json::Value calendar;
		calendar["calendar_id"] = row["id"].as< std::int64_t >();
		calendar["source_type"] = row["type_id"].as< std::string >();
		calendar["external_id"] = row["external_id"].as< std::string >();
		calendar["metadata"] = field_json(row, "metadata");
		calendar["created_at"] = row["created_at"].as< std::string >();
		calendar["visibility"] = row["visibility"].as< std::string >();
		return calendar;
	}

	api::response_t calendars_ctrl::database_error(const drogon::orm::DrogonDbException& error) const
	{
		return database_error(error.base());
	}

	api::response_t calendars_ctrl::database_error(const std::exception& error) const
	{
		LOG_ERROR << "Calendars database error: " << error.what();
		return resp.k500_database_error;
	}
}
