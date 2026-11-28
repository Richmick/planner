#include "calendars_ctrl.hpp"

#include "calendar_read_access.hpp"
#include "database_utils.hpp"
#include "scheduler/ruz_scheduler.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>

namespace api::calendars
{
	namespace
	{
		struct saved_calendar
		{
			drogon::HttpStatusCode status;
			std::int64_t id = -1;
			std::string name;
		};

		drogon::Task< saved_calendar > resolve_calendar_conflict(drogon::orm::Transaction& transaction,
				bool postgres, const std::string& source_type, const std::string& external_id,
				std::int64_t user_id, const std::string& name, const std::string& metadata)
		{
			std::string query = "SELECT id, visibility, owner_id, metadata->>'name' AS name "
					"FROM calendars WHERE type_id = ? AND external_id = ?";
			if (postgres) query += " FOR UPDATE";
			const auto rows = co_await transaction.execSqlCoro(db::query(query, postgres), source_type, external_id);
			if (rows.empty()) co_return saved_calendar{drogon::k404NotFound};

			const auto row = rows.front();
			const auto id = row["id"].as< std::int64_t >();
			const auto visibility = row["visibility"].as< std::string >();
			const bool owner = !row["owner_id"].isNull() && row["owner_id"].as< std::int64_t >() == user_id;
			if (!owner && visibility == "private") co_return saved_calendar{drogon::k404NotFound};

			bool can_write = owner;
			if (!owner && visibility == "protected")
			{
				std::string grant_query = "SELECT access FROM calendar_access WHERE calendar_id = ? AND user_id = ?";
				if (postgres) grant_query += " FOR SHARE";
				const auto grants = co_await transaction.execSqlCoro(db::query(grant_query, postgres), id, user_id);
				if (grants.empty()) co_return saved_calendar{drogon::k403Forbidden};
				const auto access = grants.front()["access"].as< std::string >();
				can_write = access == "w" || access == "s";
			}

			if (can_write)
			{
				co_await transaction.execSqlCoro(
						db::query("UPDATE calendars SET metadata = ?{json} WHERE id = ?", postgres), metadata, id);
				co_return saved_calendar{drogon::k201Created, id, name};
			}
			const auto saved_name = row["name"].isNull() ? external_id : row["name"].as< std::string >();
			co_return saved_calendar{drogon::k201Created, id, saved_name};
		}

		drogon::Task< saved_calendar > save_calendar(const std::string& source_type,
				const std::string& external_id, const std::string& name, std::int64_t user_id)
		{
			Json::Value metadata;
			metadata["name"] = name;
			Json::StreamWriterBuilder writer;
			writer["indentation"] = "";
			const std::string metadata_json = Json::writeString(writer, metadata);

			auto db_client = drogon::app().getDbClient();
			const bool postgres = db_client->type() == drogon::orm::ClientType::PostgreSQL;
			const std::string insert_calendar = db::query(
					"INSERT INTO calendars (type_id, external_id, metadata, owner_id)"
					" SELECT id, ?, ?{json}, ? FROM calendar_types WHERE id = ?"
					" ON CONFLICT (type_id, external_id) DO NOTHING RETURNING id", postgres);
			const std::string insert_subscription = db::query(
					"INSERT INTO subscriptions (user_id, calendar_id) VALUES (?, ?)"
					" ON CONFLICT (user_id, calendar_id) DO NOTHING", postgres);
			std::optional< std::int64_t > owner;
			if (source_type != "ruz") owner = user_id;
			auto transaction = co_await db_client->newTransactionCoro();
			saved_calendar saved{drogon::k201Created, -1, name};
			try
			{
				const auto rows = co_await transaction->execSqlCoro(insert_calendar, external_id, metadata_json, owner,
						source_type);
				if (rows.empty())
				{
					saved = co_await resolve_calendar_conflict(*transaction, postgres, source_type, external_id, user_id,
							name, metadata_json);
					if (saved.status != drogon::k201Created)
					{
						transaction->rollback();
						co_return saved;
					}
				}
				else
					saved.id = rows.front()["id"].as< std::int64_t >();
				co_await transaction->execSqlCoro(insert_subscription, user_id, saved.id);
			}
			catch (...)
			{
				transaction->rollback();
				throw;
			}
			co_await db::commit(transaction);
			co_return saved;
		}
	}

	void calendars_ctrl::initPathRouting()
	{
		ADD_METHOD_TO(calendars_ctrl::get_sources, "/api/calendars/sources", drogon::Get, drogon::Options);
		ADD_METHOD_TO(calendars_ctrl::create_source, "/api/calendars/sources", drogon::Post, drogon::Options);
		ADD_METHOD_TO(calendars_ctrl::get_calendars, "/api/calendars/calendars", drogon::Get, drogon::Options);
		ADD_METHOD_TO(calendars_ctrl::create_calendar, "/api/calendars/calendars", drogon::Post, drogon::Options);
		ADD_METHOD_TO(calendars_ctrl::get_calendar, "/api/calendars/calendars/{1}", drogon::Get);
		ADD_METHOD_TO(calendars_ctrl::grant_calendar_access, "/api/calendars/calendars/{1}/access", drogon::Post);
		ADD_METHOD_TO(calendars_ctrl::request_calendar_access, "/api/notifier/events", drogon::Post);
		ADD_METHOD_TO(calendars_ctrl::get_events, "/api/calendars/events", drogon::Get, drogon::Options);
		ADD_METHOD_TO(calendars_ctrl::get_active_events, "/api/calendars/events/active", drogon::Get, drogon::Options);
	}

	drogon::Task<> calendars_ctrl::get_calendars(api::request_t request, api::response_callback callback)
	{
		const std::int64_t user_id = parse_id(request->getHeader("user-id"));
		if (user_id == -1) co_return callback(resp.k401_unauthorized);

		try
		{
			std::string sql = "SELECT c.id, c.type_id, c.external_id, c.metadata, c.visibility, ";
			sql += calendar_created_at_sql(db::is_postgresql());
			sql += " AS created_at FROM calendars c JOIN subscriptions s ON s.calendar_id = c.id"
					" WHERE s.user_id = ?";
			sql += subscribed_calendar_read_access;
			sql += " ORDER BY c.created_at DESC, c.id DESC";
			const auto rows = co_await drogon::app().getDbClient()->execSqlCoro(db::query(sql), user_id);
			Json::Value calendars(Json::arrayValue);
			for (const auto& row: rows)
				calendars.append(calendar_json(row));
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

		saved_calendar saved{drogon::k404NotFound};
		try
		{
			saved = co_await save_calendar(source_type, external_id, name, user_id);
		}
		catch (const drogon::orm::DrogonDbException& error)
		{
			co_return callback(database_error(error));
		}
		catch (const std::exception& error)
		{
			co_return callback(database_error(error));
		}
		if (saved.status == drogon::k404NotFound) co_return callback(resp.k404_not_found);
		if (saved.status == drogon::k403Forbidden) co_return callback(resp.k403_forbidden);

		if (group_id != -1)
		{
			try
			{
				ruz::sync_calendar(saved.id, group_id);
			}
			catch (const std::exception& error)
			{
				LOG_ERROR << "Failed to queue RUZ sync for calendar " << saved.id << ": " << error.what();
			}
		}
		Json::Value result;
		result["calendar_id"] = saved.id;
		result["source_type"] = source_type;
		result["external_id"] = external_id;
		result["name"] = saved.name;
		co_return callback(api::response::new_json(std::move(result), drogon::k201Created));
	}
}
