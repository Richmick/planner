#include "profiles.hpp"

#include <sodium.h>
#include <http_utils.hpp>

using api::operator""_s;

void api::users::ctrl::profiles::initPathRouting()
{
	ADD_METHOD_TO(profiles::get_one,"/api/users/{1}", drogon::Get, drogon::Options);
	//ADD_METHOD_TO(profiles::update_one,"/api/users/{1}", drogon::Put, drogon::Options);
	//ADD_METHOD_TO(profiles::delete_one,"/api/users/{1}", drogon::Delete, drogon::Options);
	//ADD_METHOD_TO(profiles::subscribe, "/api/users/{1}/subscribe", drogon::Post, drogon::Options);

	ADD_METHOD_TO(profiles::login, "/api/login", drogon::Post, drogon::Options);
	ADD_METHOD_TO(profiles::create_one,"/api/profiles/register", drogon::Post, drogon::Options);
}

drogon::Task<> api::users::ctrl::profiles::get_one(api::request_t req,
			api::response_callback callback, id_t id)
{
	drogon::orm::DbClientPtr sql_client = drogon::app().getDbClient();
	drogon::orm::Result req_res;
	try
	{
		req_res = co_await sql_client->execSqlCoro("SELECT nickname, email FROM profiles WHERE id = $1;", id);
	}
	catch (const drogon::orm::DrogonDbException& e)
	{
		LOG_ERROR << "Database error: " << e.base().what();
		callback(api::response::new_error(drogon::HttpStatusCode::k500InternalServerError,
					"DB_ERROR"_s, "Internal database error"_s));
		co_return;
	}
	if (req_res.size() < 1)
	{
		callback(api::response::new_error(drogon::HttpStatusCode::k404NotFound,
					"NOT_FOUND"_s, "Not found or forbidden"_s));
		co_return;
	}
	if (req_res.size() > 1)
	{
		LOG_ERROR << "User with id " << id << " found multiple times";
	}
	Json::Value res;
	res["nickname"] = req_res.front()[0].c_str();
	res["email"] = req_res.front()[1].c_str();
	auto response = drogon::HttpResponse::newHttpJsonResponse(std::move(res));
	callback(response);
}
drogon::Task<> api::users::ctrl::profiles::login(api::request_t req, api::response_callback callback)
{
	api::expected< login_data > body = parse_login(req);
	if (!body)
	{
		callback(body.error());
		co_return;
	}
	drogon::orm::DbClientPtr sql_client = drogon::app().getDbClient();
	drogon::orm::Result req_res;
	try
	{
		req_res = co_await sql_client->execSqlCoro("SELECT id, password FROM profiles WHERE nickname = $1;", body->nickname);
	}
	catch (const drogon::orm::DrogonDbException& e)
	{
		LOG_ERROR << "Database error: " << e.base().what();
		callback(api::response::new_error(drogon::HttpStatusCode::k500InternalServerError,
					"DB_ERROR"_s, "Internal database error"_s));
		co_return;
	}
	if (req_res.size() >= 1)
	{
		if (req_res.size() > 1)
		{
			LOG_ERROR << "User with nickname " << body->nickname << " found multiple times";
		}
		if (crypto_pwhash_str_verify(req_res.front()[1].c_str(), body->password.c_str(), body->password.length()) == 0)
		{
			Json::Value res;
			res["id"] = req_res.front()[0].as< std::int64_t >();
			res["nickname"] = body->nickname;
			callback(drogon::HttpResponse::newHttpJsonResponse(std::move(res)));
			co_return;
		}
	}
	callback(api::response::new_400("FAILED"_s, "wrong login or password"_s));
}
drogon::Task<> api::users::ctrl::profiles::create_one(api::request_t req, api::response_callback callback)
{
	api::expected< register_data > body = parse_register(req);
	if (!body) co_return callback(body.error());
	std::optional< std::string > pw_hash = hash_password(body->password);
	if (!pw_hash) co_return callback(resp.k400_unsupported_password);

	drogon::orm::DbClientPtr sql_client = drogon::app().getDbClient();
	drogon::orm::Result req_res;
	try
	{
		req_res = co_await sql_client->execSqlCoro("INSERT INTO profiles(nickname,email,password)"
					"VALUES($1,$2,$3);", body->nickname, body->email, *pw_hash);
	}
	catch (const drogon::orm::UniqueViolation& e)
	{
		std::string_view err = e.what();
		if (err.contains("email"))
		{
			co_return callback(resp.k400_email_busy);
		}
		co_return callback(resp.k400_nickname_busy);
	}
	catch (const drogon::orm::DrogonDbException& e)
	{
		LOG_ERROR << "Database error: " << e.base().what();
		co_return callback(resp.k500_db_error);
	}
	co_return callback(resp.k204_ok);
}

api::expected< api::users::login_data> api::users::ctrl::profiles::parse_login(const api::request_t& req)
{
	const std::shared_ptr< Json::Value >& body = req->getJsonObject();
	if (!body || !body->isObject())
	{
		return api::unexpected{resp.k400_not_json};
	}
	login_data res;
	res.nickname = body->get("nickname", "").asString();
	res.password = body->get("password", "").asString();
	if (res.nickname.empty() || res.password.empty())
	{
		return api::unexpected{resp.k400_missing_obligatory_login};
	}
	return res;
}
api::expected< api::users::register_data> api::users::ctrl::profiles::parse_register(const api::request_t& req)
{
	const std::shared_ptr< Json::Value >& body = req->getJsonObject();
	if (!body || !body->isObject())
	{
		return api::unexpected{resp.k400_not_json};
	}
	register_data res;
	res.nickname = body->get("nickname", "").asString();
	res.email = body->get("email", "").asString();
	res.password = body->get("password", "").asString();
	if (res.nickname.empty() || res.email.empty() || res.password.empty())
	{
		return api::unexpected{resp.k400_missing_obligatory_register};
	}
	if (res.password.length() < pw_min_length)
	{
		return api::unexpected{resp.k400_password_too_short};
	}
	return res;
}
