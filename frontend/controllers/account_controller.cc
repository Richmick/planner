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
drogon::Task<> account::controller::pass_self(drogon::HttpRequestPtr request, drogon::AdviceCallback callback,
			std::size_t target_id)
{
	drogon::HttpClientPtr user_service = innerplane::users.get(request);
	auto user = request->session()->get< data::user_t >("user");
	if (!user.is_admin && (user.id != target_id))
	{
		api::send_error(std::move(callback), drogon::k403Forbidden, "NOT_ADMIN", "requires admin");
		co_return;
	}
	request->setPassThrough(true);
	callback(co_await user_service->sendRequestCoro(request));
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
		callback(gen_register_page(request->session(), error::EMPTY_FIELDS));
		co_return;
	}
	if (password != password_confirm)
	{
		callback(gen_register_page(request->session(), error::PASSWORDS_MISMATCH));
		co_return;
	}
	if (password.length() < 8)
	{
		callback(gen_register_page(request->session(), error::TOO_SHORT_PW));
		co_return;
	}
	if (!data::is_login(login) || !data::is_email(email))
	{
		callback(gen_register_page(request->session(), error::WRONG_FORMAT));
		co_return;
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
		callback(default_redirect(user_id));
		co_return;
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
		callback(gen_login_page(request->session(), error::EMPTY_FIELDS));
		co_return;
	}
	if (!data::is_login(login))
	{
		callback(gen_login_page(request->session(), error::WRONG_FORMAT));
		co_return;
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
		callback(default_redirect(user_id));
		co_return;
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
