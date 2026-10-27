#include "events_ctrl.h"
#include <string>
#include <http_utils.h>

void api::restful::events_ctrl::get_one(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
			Events::PrimaryKeyType&& id)
{
	base_type::getOne(req, std::move(callback), std::move(id));
}
void api::restful::events_ctrl::update_one(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
			Events::PrimaryKeyType&& id)
{
	base_type::updateOne(req, std::move(callback), std::move(id));
}
void api::restful::events_ctrl::delete_one(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
			Events::PrimaryKeyType&& id)
{
	base_type::deleteOne(req, std::move(callback), std::move(id));
}
void api::restful::events_ctrl::create(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
	base_type::create(req, std::move(callback));
}

void api::restful::events_ctrl::get(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
	std::string user_id_str = req->getParameter("user_id");
	if (user_id_str.empty()) {
		return callback(api::response::new_400(Json::StaticString("MISSING_USER_ID"), Json::StaticString("user_id query parameter is required")));
	}

	drogon::orm::DbClientPtr sql_client = drogon::app().getDbClient();
	
	std::string sql = 
		"SELECT e.event_id, e.name, "
		"COALESCE(o.local_scheduled, e.global_scheduled) AS effective_scheduled, "
		"o.confirmed, COALESCE(o.finished, e.finished) AS is_finished, "
		"COALESCE(o.success, false) AS is_success, "
		"COALESCE(o.dependencies, e.base_dependencies) AS effective_deps, "
		"COALESCE(o.restrictions, e.base_restrictions) AS effective_restrictions, "
		"e.overlap, e.interruptible, e.version "
		"FROM events e "
		"JOIN subscriptions s ON s.calendar_id = e.calendar_id "
		"LEFT JOIN event_overrides o ON o.event_id = e.event_id AND o.user_id = s.user_id "
		"WHERE s.user_id = $1 AND e.finish_before > NOW() "
		"ORDER BY (COALESCE(o.local_scheduled, e.global_scheduled)->0->>'start')::timestamptz";

	sql_client->execSqlAsync(sql,
		[callback](const drogon::orm::Result& result) {
			Json::Value events(Json::arrayValue);
			for (const auto& row : result) {
				Json::Value ev;
				ev["event_id"] = row["event_id"].as<std::int64_t>();
				ev["name"] = row["name"].as<std::string>();
				ev["effective_scheduled"] = row["effective_scheduled"].as<std::string>();
				ev["confirmed"] = row["confirmed"].isNull() ? Json::Value(Json::nullValue) : Json::Value(row["confirmed"].as<std::string>());
				ev["is_finished"] = row["is_finished"].as<bool>();
				ev["is_success"] = row["is_success"].as<bool>();
				ev["effective_deps"] = row["effective_deps"].isNull() ? Json::Value(Json::nullValue) : Json::Value(row["effective_deps"].as<std::string>());
				ev["effective_restrictions"] = row["effective_restrictions"].isNull() ? Json::Value(Json::nullValue) : Json::Value(row["effective_restrictions"].as<std::string>());
				ev["overlap"] = row["overlap"].as<std::string>();
				ev["interruptible"] = row["interruptible"].as<bool>();
				ev["version"] = row["version"].as<int>();
				events.append(ev);
			}
			callback(api::response::new_json(events));
		},
		[callback](const drogon::orm::DrogonDbException& e) {
			callback(api::response::new_error(drogon::k500InternalServerError, "DB_ERROR", e.base().what()));
		},
		user_id_str
	);
}
