#include "users_ctrl.h"

#include <string>
#include <sodium.h>
#include <http_utils.h>

using namespace api::response;
using api::operator""_s;

long long api::restful::users_ctrl::now()
{
	auto stamp = std::chrono::system_clock::now().time_since_epoch();
	auto stamp_seconds = std::chrono::duration_cast< std::chrono::seconds >(stamp);
	return stamp_seconds.count();
}

void api::restful::users_ctrl::get_one(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Users::PrimaryKeyType&& id)
{
	drogon::orm::DbClientPtr sql_client = drogon::app().getDbClient();
	drogon::orm::Mapper< Users > mapper(sql_client);
	try
	{
		Users user = mapper.findByPrimaryKey(id);
		Json::Value subscribes = prepare_array(user.getSubscribes());
		user.setSubscribesToNull();
		Json::Value json = user.toJson();
		json[Users::Cols::_subscribes] = std::move(subscribes);
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
void api::restful::users_ctrl::update_one(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Users::PrimaryKeyType&& id)
{
	const auto& json = req->getJsonObject();
	if (!json || !json->isObject())
	{
		return callback(new_400("NOT_JSON"_s, "expected json object"_s));
	}
	drogon::orm::DbClientPtr sql_client = drogon::app().getDbClient();
	drogon::orm::Mapper< Users > mapper(sql_client);
	const auto& pw_tag = Users::Cols::_password;
	const Json::Value* password = json->find(pw_tag.c_str(), pw_tag.c_str() + pw_tag.length());
	if (password != nullptr)
	{
		Json::String password_str = password->asString();
		if (!json->isMember("previous_password"))
			return callback(new_400("MISSING_OBLIGATORY"_s, "missing previous password"_s));
		Json::String prev_pw = json->get("previous_password", "").asString();
		if ((prev_pw.length() < 8) || (password_str.length() < 8))
			return callback(new_400("TOO_SHORT_PW"_s, "password must be at least 8 chars length"_s));
		try
		{
			Users user = mapper.findByPrimaryKey(id);
			if (const auto& user_password = user.getPassword();
						!user_password || crypto_pwhash_str_verify(user_password->c_str(),
							prev_pw.c_str(), prev_pw.length()) != 0)
			{
				return callback(new_400("FAILED"_s, "wrong login or password"_s));
			}
		}
		catch (const drogon::orm::UnexpectedRows&)
		{
			return callback(new_400("FAILED"_s, "wrong login or password"_s));
		}
	}

	json->removeMember(Users::Cols::_id);
	json->removeMember(Users::Cols::_subscribes);
	json->removeMember(Users::Cols::_last_activity);
	Users user{*json};
	user.setId(id);

	try
	{
		std::size_t affected = mapper.update(user);
		if (affected == 0)
			return callback(new_error(drogon::k404NotFound, "NOT_EXISTS"_s, "unknown user"_s));
		else
			return callback(new_response(drogon::k202Accepted));
	}
	catch (const drogon::orm::UniqueViolation&)
	{
		return callback(new_400("NOT_UNIQUE"_s, "not unique"_s));
	}
}
void api::restful::users_ctrl::delete_one(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Users::PrimaryKeyType&& id)
{
	drogon::orm::DbClientPtr sql_client = drogon::app().getDbClient();
	drogon::orm::Mapper< Users > mapper(sql_client);
	std::size_t affected = mapper.deleteByPrimaryKey(id);
	if (affected == 0)
		return callback(new_error(drogon::k404NotFound, "NOT_EXISTS"_s, "unknown user"_s));
	else
		return callback(new_response());
}

void api::restful::users_ctrl::subscribe(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Users::PrimaryKeyType&& id)
{
	const auto& json = req->getJsonObject();
	if (!json || !json->isArray())
	{
		return callback(new_400("NOT_JSON"_s, "expected json array"_s));
	}
	drogon::orm::DbClientPtr sql_client = drogon::app().getDbClient();
	drogon::orm::Mapper< Users > mapper(sql_client);

	Users user;
	user.setId(id);
	try
	{
		user.setSubscribes(parse_array(*json));
	}
	catch (const Json::Exception& e)
	{
		return callback(new_400("WRONG_JSON_FORMATS", e.what()));
	}

	std::size_t affected = mapper.update(user);
	if (affected == 0)
		return callback(new_error(drogon::k404NotFound, "NOT_EXISTS"_s, "unknown user"_s));
	else
		return callback(new_json(*json, drogon::k202Accepted));
}
void api::restful::users_ctrl::create(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
	const auto& json = req->getJsonObject();
	if (!json || !json->isObject())
	{
		return callback(new_400("NOT_JSON"_s, "expected json object"_s));
	}
	if (json->get("password", "").asString().length() < 8)
	{
		return callback(new_400("TOO_SHORT_PW"_s, "password must be at least 8 chars length"_s));
	}
	drogon::orm::DbClientPtr sql_client = drogon::app().getDbClient();
	drogon::orm::Mapper< Users > mapper(sql_client);

	json->removeMember(Users::Cols::_id);
	json->removeMember(Users::Cols::_subscribes);
	json->removeMember(Users::Cols::_last_activity);
	Users user{*json};
	user.setLastActivity(now());

	try
	{
		mapper.insert(user);
		Json::Value json = user.toJson();
		json.removeMember(Users::Cols::_password);
		callback(new_json(std::move(json), drogon::k201Created));
	}
	catch (const drogon::orm::UniqueViolation&)
	{
		return callback(new_400("FAILED"_s, "not unique"_s));
	}
}
void api::restful::users_ctrl::login(const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback)
{
	const std::shared_ptr< Json::Value >& body = request->getJsonObject();
	if (!body || !body->isObject())
	{
		return callback(new_400("NOT_JSON"_s, "expected json object"_s));
	}
	std::string nickname = body->get("nickname", "").asString();
	std::string password = body->get("password", "").asString();
	if (nickname.empty() || password.empty())
	{
		return callback(new_400("MISSING_OBLIGATORY"_s, "missing login or password in json"_s));
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

			Json::Value subscribes = prepare_array(user.getSubscribes());
			user.setSubscribesToNull();
			Json::Value result = user.toJson();
			result[Users::Cols::_subscribes] = std::move(subscribes);
			result.removeMember(Users::Cols::_password);
			callback(drogon::HttpResponse::newHttpJsonResponse(std::move(result)));
			return;
		}
	}
	catch (const drogon::orm::UnexpectedRows&)
	{}
	return callback(new_400("FAILED"_s, "wrong login or password"_s));
}
