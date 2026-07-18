#include "users_ctrl.h"

#include <string>
#include <sodium.h>
#include <http_utils.h>

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
void api::restful::users_ctrl::updateOne(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Users::PrimaryKeyType&& id)
{
	const auto& json = req->getJsonObject();
	if (!json || !json->isObject())
	{
		send_400(std::move(callback), "NOT_JSON", "expected json object");
		return;
	}
	drogon::orm::DbClientPtr sql_client = drogon::app().getDbClient();
	drogon::orm::Mapper< Users > mapper(sql_client);
	const auto& pw_tag = Users::Cols::_password;
	const Json::Value* password = json->find(pw_tag.c_str(), pw_tag.c_str() + pw_tag.length());
	if (password != nullptr)
	{
		Json::String password_str = password->asString();
		if (!json->isMember("previous_password"))
		{
			send_400(std::move(callback), "MISSING_OBLIGATORY", "missing previous password");
			return;
		}
		Json::String prev_pw = json->get("previous_password", "").asString();
		if ((prev_pw.length() < 8) || (password_str.length() < 8))
		{
			send_400(std::move(callback), "TOO_SHORT_PW", "password must be at least 8 chars length");
			return;
		}
		try
		{
			Users user = mapper.findByPrimaryKey(id);
			if (const auto& user_password = user.getPassword();
						!user_password || crypto_pwhash_str_verify(user_password->c_str(),
							prev_pw.c_str(), prev_pw.length()) != 0)
			{
				send_400(std::move(callback), "FAILED", "wrong login or password");
				return;
			}
		}
		catch (const drogon::orm::UnexpectedRows&)
		{
			send_400(std::move(callback), "FAILED", "wrong login or password");
			return;
		}
	}
	json->removeMember(Users::Cols::_id);
	json->removeMember(Users::Cols::_last_activity);

	Users user{*json};
	user.setId(id);
	try
	{
		std::size_t affected = mapper.update(user);
		if (affected == 0)
			send_error(std::move(callback), drogon::k404NotFound, "NOT_EXISTS", "unknown user");
		else
			send_code_only(std::move(callback), drogon::k202Accepted);
	}
	catch (const drogon::orm::UniqueViolation&)
	{
		send_400(std::move(callback), "NOT_UNIQUE", "not unique");
	}
}
void api::restful::users_ctrl::deleteOne(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Users::PrimaryKeyType&& id)
{
	drogon::orm::DbClientPtr sql_client = drogon::app().getDbClient();
	drogon::orm::Mapper< Users > mapper(sql_client);
	std::size_t affected = mapper.deleteByPrimaryKey(id);
	if (affected == 0)
		send_error(std::move(callback), drogon::k404NotFound, "NOT_EXISTS", "unknown user");
	else
		send_success(std::move(callback));
}

void api::restful::users_ctrl::create(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
	const auto& json = req->getJsonObject();
	if (!json || !json->isObject())
	{
		send_400(std::move(callback), "NOT_JSON", "expected json object");
		return;
	}
	if (json->get("password", "").asString().length() < 8)
	{
		send_400(std::move(callback), "TOO_SHORT_PW", "password must be at least 8 chars length");
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
