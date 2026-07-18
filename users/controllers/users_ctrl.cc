#include "users_ctrl.h"

#include <string>
#include <sodium.h>

void api::restful::users_ctrl::send_400(drogon::AdviceCallback&& callback, std::string errcode, std::string errmsg)
{
	Json::Value json, err;
	err["code"] = errcode;
	err["msg"] = errmsg;
	json["error"] = std::move(err);
	auto response = drogon::HttpResponse::newHttpJsonResponse(std::move(json));
	response->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
	callback(response);
}
long long api::restful::users_ctrl::now()
{
	auto stamp = std::chrono::system_clock::now().time_since_epoch();
	auto stamp_seconds = std::chrono::duration_cast< std::chrono::seconds >(stamp);
	return stamp_seconds.count();
}

void api::restful::users_ctrl::getOne(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Users::PrimaryKeyType&& id)
{
	drogon::orm::DbClientPtr sql_client = drogon::app().getDbClient();
	drogon::orm::Mapper< Users > mapper(sql_client);
	try
	{
		Users user = mapper.findByPrimaryKey(id);
		Json::Value json = user.toJson();
		json.removeMember(Users::Cols::_password);
		callback(drogon::HttpResponse::newHttpJsonResponse(json));
	}
	catch (const drogon::orm::UnexpectedRows&)
	{
		auto response = drogon::HttpResponse::newHttpJsonResponse(Json::nullValue);
		response->setStatusCode(drogon::HttpStatusCode::k404NotFound);
		callback(response);
	}
}

void api::restful::users_ctrl::create(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
	const auto& json = req->getJsonObject();
	if (!json || !json->isObject())
	{
		send_400(std::move(callback), "NOT_JSON", "expected json object");
		return;
	}
	drogon::orm::DbClientPtr sql_client = drogon::app().getDbClient();
	drogon::orm::Mapper< Users > mapper(sql_client);
	Users user{*json};
	user.setLastActivity(now());
	try
	{
		mapper.insert(user);
		Json::Value json = user.toJson();
		json.removeMember(Users::Cols::_password);
		auto response = drogon::HttpResponse::newHttpJsonResponse(json);
		response->setStatusCode(drogon::HttpStatusCode::k201Created);
		callback(response);
	}
	catch (const drogon::orm::UniqueViolation&)
	{
		send_400(std::move(callback), "FAILED", "not unique");
	}
}
void api::restful::users_ctrl::login(const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback)
{
	const std::shared_ptr< Json::Value >& body = request->getJsonObject();
	if (!body || !body->isObject())
	{
		send_400(std::move(callback), "NOT_JSON", "expected json object");
		return;
	}
	std::string nickname = body->get("nickname", "").asString();
	std::string password = body->get("password", "").asString();
	if (nickname.empty() || password.empty())
	{
		send_400(std::move(callback), "MISSING_OBLIGATORY", "missing login or password in json");
		return;
	}
	drogon::orm::DbClientPtr sql_client = drogon::app().getDbClient();
	drogon::orm::Mapper< Users > mapper(sql_client);
	try
	{
		Users user = mapper.findOne(drogon::orm::Criteria{Users::Cols::_nickname,
					drogon::orm::CompareOperator::EQ, nickname});
		if (const auto& user_password = user.getPassword();
					user_password && crypto_pwhash_str_verify(user_password->c_str(),
						password.c_str(), password.length()) == 0)
		{
			user.setLastActivity(now());
			mapper.update(user);

			Json::Value result = user.toJson();
			result.removeMember(Users::Cols::_password);
			auto subscribes = user.getSubscribes();
			if (subscribes)
			{
				Json::Value arr{Json::arrayValue};
				std::size_t* p = reinterpret_cast< std::size_t* >(subscribes->data());
				std::size_t* end = reinterpret_cast< std::size_t* >(subscribes->data())
						+ (subscribes->size() / sizeof std::size_t);
				for (; p < end; p++) arr.append(*p);
				result[Users::Cols::_subscribes] = std::move(arr);
			}
			callback(drogon::HttpResponse::newHttpJsonResponse(result));
			return;
		}
	}
	catch (const drogon::orm::UnexpectedRows&)
	{}
	send_400(std::move(callback), "FAILED", "wrong login or password");
}
