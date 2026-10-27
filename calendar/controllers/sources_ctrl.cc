#include "sources_ctrl.h"
#include <string>
#include <http_utils.h>

void api::restful::Sources_ctrl::get_one(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Sources::PrimaryKeyType&& id)
{
	base_type::getOne(req, std::move(callback), std::move(id));
}
void api::restful::Sources_ctrl::update_one(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Sources::PrimaryKeyType&& id)
{
	base_type::updateOne(req, std::move(callback), std::move(id));
}
void api::restful::Sources_ctrl::delete_one(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Sources::PrimaryKeyType&& id)
{
	base_type::deleteOne(req, std::move(callback), std::move(id));
}
void api::restful::Sources_ctrl::get(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
	base_type::get(req, std::move(callback));
}
void api::restful::Sources_ctrl::create(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
	base_type::create(req, std::move(callback));
}

drogon::Task<> api::restful::Sources_ctrl::subscribe_user(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Sources::PrimaryKeyType&& id)
{
	using namespace api::response;
	using api::operator""_s;

	std::size_t user_id = static_cast< std::size_t >(id);
	const auto& json = req->getJsonObject();
	if (!json || !json->isObject())
		co_return callback(new_400("NOT_JSON"_s, "expected json object"_s));
	
	Json::Value create_list = json->get("create", Json::arrayValue);
	Json::Value delete_list = json->get("delete", Json::arrayValue);
	if (!create_list.isArray() || !delete_list.isArray())
		co_return callback(new_400("WRONG_JSON_TYPE"_s, "expected lists to create/delete"_s));
	
	if (create_list.size() + delete_list.size() <= 0)
		co_return callback(new_400("NO_ACTION_REQUESTED"_s, "requested update of 0 elements"_s));

	drogon::orm::DbClientPtr sql_client = drogon::app().getDbClient();
	
	try {
		auto trans = co_await sql_client->newTransactionCoro();
		
		for (const Json::Value& i : create_list) {
			std::size_t calendar_id = i.asUInt64();
			co_await trans->execSqlCoro(
				"INSERT INTO subscriptions (user_id, calendar_id) VALUES ($1, $2) ON CONFLICT DO NOTHING",
				user_id, calendar_id
			);
			co_await trans->execSqlCoro(
				"INSERT INTO outbox_events (aggregate_type, aggregate_id, event_type, payload) "
				"VALUES ('subscription', $1, 'SubscriptionCreated', jsonb_build_object('user_id', $2, 'calendar_id', $3))",
				std::to_string(user_id) + ":" + std::to_string(calendar_id), user_id, calendar_id
			);
		}
		
		for (const Json::Value& i : delete_list) {
			std::size_t calendar_id = i.asUInt64();
			co_await trans->execSqlCoro(
				"DELETE FROM subscriptions WHERE user_id = $1 AND calendar_id = $2",
				user_id, calendar_id
			);
			co_await trans->execSqlCoro(
				"INSERT INTO outbox_events (aggregate_type, aggregate_id, event_type, payload) "
				"VALUES ('subscription', $1, 'SubscriptionDeleted', jsonb_build_object('user_id', $2, 'calendar_id', $3))",
				std::to_string(user_id) + ":" + std::to_string(calendar_id), user_id, calendar_id
			);
		}
		
		co_await trans->commitCoro();
		
		Json::Value response{Json::arrayValue};
		auto res = co_await sql_client->execSqlCoro(
			"SELECT calendar_id FROM subscriptions WHERE user_id = $1", user_id
		);
		for (const auto& row : res) response.append(row[0].as<std::size_t>());
		
		callback(new_json(std::move(response), drogon::k202Accepted));
	}
	catch (const drogon::orm::DrogonDbException& e) {
		callback(new_error(drogon::k500InternalServerError, "DB_ERROR"_s, e.base().what()));
	}
}
