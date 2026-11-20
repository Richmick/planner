#include "calendars_ctrl.hpp"

#include "database_utils.hpp"
#include "scheduler/ruz_scheduler.hpp"

#include <stdexcept>

namespace api::calendars
{
	drogon::Task<> calendars_ctrl::get_sources(api::request_t, api::response_callback callback)
	{
		try
		{
			const auto rows = co_await drogon::app().getDbClient()->execSqlCoro(
					"SELECT id, description FROM calendar_types ORDER BY id");
			Json::Value sources(Json::arrayValue);
			for (const auto& row: rows)
			{
				Json::Value source;
				source["type_id"] = row["id"].as< std::string >();
				source["description"] = row["description"].as< std::string >();
				sources.append(std::move(source));
			}
			co_return callback(api::response::new_json(std::move(sources)));
		}
		catch (const drogon::orm::DrogonDbException& error)
		{
			co_return callback(database_error(error));
		}
	}

	drogon::Task<> calendars_ctrl::create_source(api::request_t request, api::response_callback callback)
	{
		const auto& body = request->getJsonObject();
		if (!body || !body->isObject()) co_return callback(resp.k400_invalid_body);

		const Json::Value type = body->get("type_id", "");
		const Json::Value description = body->get("description", "");
		if (!type.isString() || !valid_source_type(type.asString()))
			co_return callback(resp.k400_invalid_source_type);
		if (!description.isString() || description.asString().find('\0') != std::string::npos)
			co_return callback(resp.k400_invalid_description);

		try
		{
			const auto rows = co_await drogon::app().getDbClient()->execSqlCoro(db::query(
					"INSERT INTO calendar_types (id, description) VALUES (?, ?)"
					" ON CONFLICT (id) DO UPDATE SET description = excluded.description"
					" RETURNING id, description"), type.asString(), description.asString());
			Json::Value source;
			source["type_id"] = rows.front()["id"].as< std::string >();
			source["description"] = rows.front()["description"].as< std::string >();
			co_return callback(api::response::new_json(std::move(source), drogon::k201Created));
		}
		catch (const drogon::orm::DrogonDbException& error)
		{
			co_return callback(database_error(error));
		}
	}

	drogon::Task< std::int64_t > calendars_ctrl::find_ruz_group(const std::string& name)
	{
		const Json::Value result = co_await ruz::search_groups(name);
		if (!result.isObject() || !result["groups"].isArray())
			throw std::runtime_error("RUZ search response is missing the groups array");
		for (const auto& group: result["groups"])
		{
			if (!group.isObject() || !group["name"].isString() || group["name"].asString() != name)
				continue;
			const Json::Value& id = group["id"];
			if (id.isInt64() && id.asInt64() > 0) co_return id.asInt64();
			if (id.isString())
			{
				const std::int64_t group_id = parse_id(id.asString());
				if (group_id != -1) co_return group_id;
			}
			throw std::runtime_error("RUZ returned a group without a valid ID");
		}
		co_return -1;
	}
}
