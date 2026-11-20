#include "calendars_ctrl.hpp"

#include "database_utils.hpp"
#include "scheduler/ruz_scheduler.hpp"

#include <algorithm>
#include <stdexcept>

namespace api::calendars
{
	namespace
	{
		drogon::Task< std::int64_t > save_calendar(const std::string& source_type,
				const std::string& external_id, const std::string& name, std::int64_t user_id)
		{
			Json::Value metadata;
			metadata["name"] = name;
			Json::StreamWriterBuilder writer;
			writer["indentation"] = "";
			const std::string metadata_json = Json::writeString(writer, metadata);

			auto db_client = drogon::app().getDbClient();
			const std::string insert_calendar = db::query(
					"INSERT INTO calendars (type_id, external_id, metadata)"
					" SELECT id, ?, ?{json} FROM calendar_types WHERE id = ?"
					" ON CONFLICT (type_id, external_id) DO UPDATE SET metadata = excluded.metadata"
					" RETURNING id");
			const std::string insert_subscription = db::query(
					"INSERT INTO subscriptions (user_id, calendar_id) VALUES (?, ?)"
					" ON CONFLICT (user_id, calendar_id) DO NOTHING");
			auto transaction = co_await db_client->newTransactionCoro();
			std::int64_t id;
			try
			{
				const auto rows = co_await transaction->execSqlCoro(insert_calendar, external_id, metadata_json, source_type);
				if (rows.empty())
				{
					transaction->rollback();
					co_return -1;
				}
				id = rows.front()["id"].as< std::int64_t >();
				co_await transaction->execSqlCoro(insert_subscription, user_id, id);
			}
			catch (...)
			{
				transaction->rollback();
				throw;
			}
			co_await db::commit(transaction);
			co_return id;
		}
	}

	void calendars_ctrl::initPathRouting()
	{
		ADD_METHOD_TO(calendars_ctrl::get_sources, "/api/calendars/sources", drogon::Get, drogon::Options);
		ADD_METHOD_TO(calendars_ctrl::create_source, "/api/calendars/sources", drogon::Post, drogon::Options);
		ADD_METHOD_TO(calendars_ctrl::get_calendars, "/api/calendars/calendars", drogon::Get, drogon::Options);
		ADD_METHOD_TO(calendars_ctrl::create_calendar, "/api/calendars/calendars", drogon::Post, drogon::Options);
		ADD_METHOD_TO(calendars_ctrl::get_events, "/api/calendars/events", drogon::Get, drogon::Options);
		ADD_METHOD_TO(calendars_ctrl::get_active_events, "/api/calendars/events/active", drogon::Get, drogon::Options);
	}

	drogon::Task<> calendars_ctrl::get_calendars(api::request_t request, api::response_callback callback)
	{
		const std::int64_t user_id = parse_id(request->getHeader("user-id"));
		if (user_id == -1) co_return callback(resp.k401_unauthorized);

		try
		{
			std::string sql = "SELECT c.id, c.type_id, c.external_id, c.metadata, ";
			sql += db::is_postgresql()
					? "to_char(c.created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"')"
					: "strftime('%Y-%m-%dT%H:%M:%fZ', c.created_at)";
			sql += " AS created_at FROM calendars c JOIN subscriptions s ON s.calendar_id = c.id"
					" WHERE s.user_id = ? ORDER BY c.created_at DESC, c.id DESC";
			const auto rows = co_await drogon::app().getDbClient()->execSqlCoro(db::query(sql), user_id);
			Json::Value calendars(Json::arrayValue);
			for (const auto& row: rows)
			{
				Json::Value calendar;
				calendar["calendar_id"] = row["id"].as< std::int64_t >();
				calendar["source_type"] = row["type_id"].as< std::string >();
				calendar["external_id"] = row["external_id"].as< std::string >();
				calendar["metadata"] = field_json(row, "metadata");
				calendar["created_at"] = row["created_at"].as< std::string >();
				calendars.append(std::move(calendar));
			}
			co_return callback(api::response::new_json(std::move(calendars)));
		}
		catch (const drogon::orm::DrogonDbException& error)
		{
			co_return callback(database_error(error));
		}
	}

	drogon::Task<> calendars_ctrl::create_calendar(api::request_t request, api::response_callback callback)
	{
		const std::int64_t user_id = parse_id(request->getHeader("user-id"));
		if (user_id == -1) co_return callback(resp.k401_unauthorized);

		const auto& body = request->getJsonObject();
		if (!body || !body->isObject()) co_return callback(resp.k400_invalid_body);

		const Json::Value type = body->get("source_type", "ruz");
		if (!type.isString() || !valid_source_type(type.asString()))
			co_return callback(resp.k400_invalid_source_type);
		const std::string source_type = type.asString();

		const Json::Value value = body->get("name", body->get("group", ""));
		if (!value.isString() || value.asString().empty()) co_return callback(resp.k400_invalid_name);
		const std::string name = value.asString();
		std::string external_id;
		std::int64_t group_id = -1;
		if (source_type == "ruz")
		{
			try
			{
				group_id = co_await find_ruz_group(name);
			}
			catch (const std::exception& error)
			{
				LOG_WARN << "RUZ group lookup failed: " << error.what();
				co_return callback(resp.k502_source_error);
			}
			if (group_id == -1) co_return callback(resp.k404_not_found);
			external_id = std::to_string(group_id);
		}
		else
		{
			const Json::Value id = body->get("external_id", "");
			if (!id.isString()) co_return callback(resp.k400_invalid_external_id);
			external_id = id.asString();
			const auto characters = std::ranges::count_if(external_id, [](unsigned char c) { return (c & 0xc0) != 0x80; });
			if (external_id.empty() || external_id.find('\0') != std::string::npos || characters > 255)
				co_return callback(resp.k400_invalid_external_id);
		}

		std::int64_t calendar_id;
		try
		{
			calendar_id = co_await save_calendar(source_type, external_id, name, user_id);
		}
		catch (const drogon::orm::DrogonDbException& error)
		{
			co_return callback(database_error(error));
		}
		catch (const std::exception& error)
		{
			co_return callback(database_error(error));
		}
		if (calendar_id == -1) co_return callback(resp.k404_not_found);

		if (group_id != -1)
		{
			try
			{
				ruz::sync_calendar(calendar_id, group_id);
			}
			catch (const std::exception& error)
			{
				LOG_ERROR << "Failed to queue RUZ sync for calendar " << calendar_id << ": " << error.what();
			}
		}
		Json::Value result;
		result["calendar_id"] = calendar_id;
		result["source_type"] = source_type;
		result["external_id"] = external_id;
		result["name"] = name;
		co_return callback(api::response::new_json(std::move(result), drogon::k201Created));
	}
}
