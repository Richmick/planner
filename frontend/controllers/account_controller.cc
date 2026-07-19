#include "account_controller.h"

#include <http_utils.h>
#include <data/user.h>
#include <data/net.h>

namespace account
{
	static drogon::HttpResponsePtr default_redirect(std::size_t user_id)
	{
		return drogon::HttpResponse::newRedirectionResponse("/" + std::to_string(user_id) + "/calendar");
	}
	static std::size_t use_user(const drogon::SessionPtr& session, const Json::Value& json)
	{
		std::size_t id = json.get("id", -1).asUInt64();
		data::user_t user = {.id = id, .login = json.get("nickname", "").asString()};
		user.last_login = data::user_t::timepoint_t{std::chrono::seconds{json.get("last_login", 0).asInt64()}};
		const Json::Value& subs = json.get("subscribes", Json::arrayValue);
		if (subs.isArray())
		{
			user.subscribes.reserve(subs.size());
			for (const Json::Value& i: subs)
			{
				user.subscribes.push_back(i.asUInt64());
			}
		}
		session->erase("user");
		session->insert("user", std::move(user));
		return id;
	}
}

drogon::Task<> account::controller::pass_login(drogon::HttpRequestPtr request, drogon::AdviceCallback callback)
{
	drogon::HttpClientPtr user_service = innerplane::users.get(request);
	request->setPassThrough(true);
	drogon::HttpResponsePtr response = co_await user_service->sendRequestCoro(request);
	std::size_t user_id;
	parse_user(request->session(), response, user_id);
	callback(response);
}
drogon::Task<> account::controller::pass(drogon::HttpRequestPtr request, drogon::AdviceCallback callback)
{
	drogon::HttpClientPtr user_service = innerplane::users.get(request);
	request->setPassThrough(true);
	callback(co_await user_service->sendRequestCoro(request));
}
drogon::Task<> account::controller::pass_self(drogon::HttpRequestPtr request,
			drogon::AdviceCallback callback, std::size_t target_id)
{
	using namespace api::response;
	using api::operator""_s;

	drogon::HttpClientPtr user_service = innerplane::users.get(request);
	auto user = request->session()->get< data::user_t >("user");
	if (!user.is_admin && (user.id != target_id))
	{
		callback(new_error(drogon::k403Forbidden, "NOT_ADMIN"_s, "requires admin"_s));
		co_return;
	}
	request->setPassThrough(true);
	callback(co_await user_service->sendRequestCoro(request));
}

drogon::Task<> account::controller::pass_subscribe(drogon::HttpRequestPtr request,
			drogon::AdviceCallback callback, std::size_t target_id)
{
	using namespace api::response;
	using api::operator""_s;

	drogon::HttpClientPtr user_service = innerplane::users.get(request);
	drogon::HttpClientPtr calendar_service = innerplane::calendar.get(request);
	auto user = request->session()->get< data::user_t >("user");
	if (!user.is_admin && (user.id != target_id))
	{
		co_return callback(new_error(drogon::k403Forbidden, "NOT_ADMIN"_s, "requires admin"_s));
	}
	const auto& json = request->getJsonObject();
	if (!json || !json->isArray())
	{
		co_return callback(new_400("NOT_JSON"_s, "expected json array"_s));
	}
	std::vector< std::size_t > requested;
	requested.reserve(json->size());
	for (const Json::Value& i: *json)
	{
		if (!i.isUInt64())
		{
			co_return callback(new_400("WRONG_JSON_TYPE"_s, "expected lists to create/delete"_s));
		}
		requested.push_back(i.asUInt64());
	}
	std::sort(requested.begin(), requested.end());
	if (requested.size() + user.subscribes.size() <= 0)
	{
		co_return callback(new_json(Json::arrayValue));
	}

	bool watch_add = (request->method() == drogon::Post) || (request->method() == drogon::Put);
	bool watch_delete = request->method() == drogon::Put;
	bool watch_overlap = request->method() == drogon::Delete;
	Json::Value to_create{Json::arrayValue}, to_delete{Json::arrayValue};
	std::vector< std::size_t > resulting_subs;
	std::size_t req_i = 0;
	for (std::size_t user_i = 0; user_i < user.subscribes.size(); user_i++)
	{
		bool add = true;
		while (req_i < requested.size())
		{
			if (user.subscribes[user_i] < requested[req_i]) break;
			if (user.subscribes[user_i] == requested[req_i++])
			{
				resulting_subs.push_back(user.subscribes[user_i]);
				add = false;
				if (watch_overlap) to_delete.append(user.subscribes[user_i]);
				else break;
			}
			if (watch_add)
			{
				to_create.append(requested[req_i - 1]);
			}
		}
		if (watch_delete && add) to_delete.append(user.subscribes[user_i]);
	}
	if (watch_add)
	{
		for (; req_i < requested.size(); req_i++)
		{
			to_create.append(requested[req_i]);
		}
	}
	if (to_create.size() + to_delete.size() <= 0)
	{
		Json::Value response{Json::arrayValue};
		for (std::size_t i: user.subscribes) response.append(i);
		co_return callback(new_json(std::move(response)));
	}

	Json::Value sub_req_json;
	if (watch_add) sub_req_json["create"] = std::move(to_create);
	if (watch_delete || watch_overlap) sub_req_json["delete"] = std::move(to_delete);
	drogon::HttpRequestPtr sub_req = drogon::HttpRequest::newHttpJsonRequest(sub_req_json);
	sub_req->setMethod(drogon::HttpMethod::Post);
	sub_req->setPath("/api/sources/renew-subscribes/" + std::to_string(target_id));
	drogon::HttpResponsePtr sub_resp = co_await calendar_service->sendRequestCoro(sub_req);
	const auto& sub_json = sub_resp->getJsonObject();
	if ((sub_resp->statusCode() >= 200) && (sub_resp->statusCode() < 300) && sub_json && sub_json->isArray())
	{
		for (const Json::Value& i: *sub_json) resulting_subs.push_back(i.asUInt64());
		user.subscribes = std::move(resulting_subs);

		Json::Value sub_req_json{Json::arrayValue};
		for (std::size_t i: user.subscribes) sub_req_json.append(i);
		request->session()->modify< data::user_t >("user",
					[user = std::move(user)](auto& u) {u = std::move(user);});
		drogon::HttpRequestPtr sub_req = drogon::HttpRequest::newHttpJsonRequest(sub_req_json);
		sub_req->setMethod(drogon::HttpMethod::Post);
		sub_req->setPath("/api/users/" + std::to_string(target_id) + "/subscribe");
		callback(co_await user_service->sendRequestCoro(sub_req));
	}
	else
	{
		callback(sub_resp);
	}
}

account::controller::error account::controller::parse_user(const drogon::SessionPtr& session,
			const drogon::HttpResponsePtr& response, std::size_t& user_id)
{
	if (response->statusCode() >= 500) return error::INTERNAL;
	const auto& json = response->getJsonObject();
	if (!json) return error::INTERNAL;
	if ((response->getStatusCode() >= 200) && (response->getStatusCode() < 300))
	{
		if (!json->isMember("id") || !json->isMember("nickname")) return error::INTERNAL;
		user_id = use_user(session, *json);
		return error::none;
	}
	Json::Value err = json->get("error", Json::nullValue);
	if (!err.isObject()) return error::INTERNAL;
	err = err.get("code", Json::nullValue);
	if (!err.isString()) return error::INTERNAL;
	std::string errcode = err.asString();

	if (err == "FAILED") return error::FAILED;
	if (err == "MISSING_OBLIGATORY") return error::EMPTY_FIELDS;
	return error::INTERNAL;
}

drogon::Task<> account::controller::register_user(drogon::HttpRequestPtr request, drogon::AdviceCallback callback)
{
	std::string login = request->getParameter("login");
	std::string email = request->getParameter("email");
	std::string password = request->getParameter("password");
	std::string password_confirm = request->getParameter("password-confirm");

	if (login.empty() || email.empty() || password.empty() || password_confirm.empty())
	{
		co_return callback(gen_register_page(request->session(), error::EMPTY_FIELDS));
	}
	if (password != password_confirm)
	{
		co_return callback(gen_register_page(request->session(), error::PASSWORDS_MISMATCH));
	}
	if (password.length() < 8)
	{
		co_return callback(gen_register_page(request->session(), error::TOO_SHORT_PW));
	}
	if (!data::is_login(login) || !data::is_email(email))
	{
		co_return callback(gen_register_page(request->session(), error::WRONG_FORMAT));
	}
	
	drogon::HttpClientPtr user_service = innerplane::users.get(request);
	Json::Value json;
	json["nickname"] = login;
	json["email"] = email;
	json["password"] = password;
	auto subreq = drogon::HttpRequest::newHttpJsonRequest(json);
	subreq->setMethod(drogon::HttpMethod::Post);
	subreq->setPath("/api/users");
	std::size_t user_id;
	error err = parse_user(request->session(), co_await user_service->sendRequestCoro(subreq), user_id);
	if (err == error::none)
	{
		LOG_INFO << "User \"" << login << "\" (ID=" << user_id << ") registered";
		co_return callback(default_redirect(user_id));
	}
	callback(gen_register_page(request->session(), err));
}
void account::controller::register_page(const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback)
{
	auto user = request->session()->getOptional< data::user_t >("user");
	if (user)
	{
		callback(default_redirect(user->id));
	}
	else
	{
		callback(gen_register_page(request->session(), error::none));
	}
}
drogon::HttpResponsePtr account::controller::gen_register_page(const drogon::SessionPtr& session, error err)
{
	drogon::HttpViewData data;
	drogon::HttpStatusCode ret_code = drogon::HttpStatusCode::k400BadRequest;
	switch (err)
	{
	case error::EMPTY_FIELDS:
		data.insertAsString("error", "Не заполнены обязательные поля");
		break;
	case error::WRONG_FORMAT:
		data.insertAsString("error", "Неверный формат логина или почты");
		break;
	case error::PASSWORDS_MISMATCH:
		data.insertAsString("error", "Пароли не совпадают");
		break;
	case error::TOO_SHORT_PW:
		data.insertAsString("error", "Слишком короткий пароль - минимальная длина - 8 символов");
		break;
	case error::FAILED:
		data.insertAsString("error", "Пользователь с данным логином или email уже существует");
		break;
	case error::UNACCEPTABLE_PW:
		data.insertAsString("error", "Ошибка в обработке пароля - введите иной");
		break;
	case error::INTERNAL:
		ret_code = drogon::HttpStatusCode::k500InternalServerError;
		data.insertAsString("error", "Внутренняя ошибка сервера");
		break;
		[[likely]]
	case error::none:
		ret_code = drogon::HttpStatusCode::k200OK;
		break;
	default:
		data.insertAsString("error", "Неизвестная ошибка");
		break;
	}
	data.insert("sessid", session->sessionId());
	auto response = drogon::HttpResponse::newHttpViewResponse("views/account/register_p.csp", data);
	response->setStatusCode(ret_code);
	return response;
}

drogon::Task<> account::controller::login(drogon::HttpRequestPtr request, drogon::AdviceCallback callback)
{
	std::string login = request->getParameter("login");
	std::string password = request->getParameter("password");
	if (login.empty() || password.empty())
	{
		co_return callback(gen_login_page(request->session(), error::EMPTY_FIELDS));
	}
	if (!data::is_login(login))
	{
		co_return callback(gen_login_page(request->session(), error::WRONG_FORMAT));
	}

	drogon::HttpClientPtr user_service = innerplane::users.get(request);
	Json::Value json;
	json["nickname"] = login;
	json["password"] = password;
	auto subreq = drogon::HttpRequest::newHttpJsonRequest(json);
	subreq->setMethod(drogon::HttpMethod::Post);
	subreq->setPath("/api/login");
	std::size_t user_id;
	error err = parse_user(request->session(), co_await user_service->sendRequestCoro(subreq), user_id);
	if (err == error::none)
	{
		LOG_INFO << "User \"" << login << "\" (ID=" << user_id << ") logged in";
		co_return callback(default_redirect(user_id));
	}
	callback(gen_login_page(request->session(), err));
}
void account::controller::login_page(const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback)
{
	auto user = request->session()->getOptional< data::user_t >("user");
	if (user)
	{
		callback(default_redirect(user->id));
	}
	else
	{
		callback(gen_login_page(request->session(), error::none));
	}
}
drogon::HttpResponsePtr account::controller::gen_login_page(const drogon::SessionPtr& session, error err)
{
	drogon::HttpViewData data;
	drogon::HttpStatusCode ret_code = drogon::HttpStatusCode::k400BadRequest;
	switch (err)
	{
	case error::EMPTY_FIELDS:
		data.insertAsString("error", "Не заполнены обязательные поля");
		break;
	case error::WRONG_FORMAT:
		data.insertAsString("error", "Неверный формат логина");
		break;
	case error::FAILED:
		data.insertAsString("error", "Неверный логин или пароль");
		break;
	case error::INTERNAL:
		ret_code = drogon::HttpStatusCode::k500InternalServerError;
		data.insertAsString("error", "Внутренняя ошибка сервера");
		break;
	[[likely]]
	case error::none:
		ret_code = drogon::HttpStatusCode::k200OK;
		break;
	default:
		data.insertAsString("error", "Неизвестная ошибка");
		break;
	}
	data.insert("sessid", session->sessionId());
	auto response = drogon::HttpResponse::newHttpViewResponse("views/account/login.csp", data);
	response->setStatusCode(ret_code);
	return response;
}

void account::controller::logout(const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback)
{
	request->session()->erase("user");
	callback(drogon::HttpResponse::newRedirectionResponse("/account/login"));
}
