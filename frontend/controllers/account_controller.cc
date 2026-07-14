#include "account_controller.h"

#include <data/user.h>

namespace account
{
	static drogon::HttpResponsePtr default_redirect(const data::user_t& user)
	{
		return drogon::HttpResponse::newRedirectionResponse("/" + std::to_string(user.id) + "/calendar");
	}
	static drogon::Task< data::user_t > get_user(drogon::orm::DbClientPtr sql_client, const std::string& login)
	{
		data::user_t user;
		drogon::orm::Result result = co_await sql_client->execSqlCoro("SELECT ID, LAST_ACTIVITY, PASSWORD"
					" FROM users WHERE NICKNAME = ?", login);
		if (result.size() != 0)
		{
			drogon::orm::Result::reference data = result[0];
			user.exist = true;
			user.id = data[0].as< std::size_t >();
			user.login = login;
			user.last_login = data::user_t::timepoint_t{std::chrono::seconds{data[1].as< long long >()}};
			std::memcpy(user.encrypted_password, data[2].c_str(), data[2].length() + 1);
		}
		co_return user;
	}
	static bool check_password(const data::user_t& user, const std::string& password)
	{
		return crypto_pwhash_str_verify(user.encrypted_password, password.c_str(), password.length()) == 0;
	}
}

drogon::Task<> account::controller::register_user(drogon::HttpRequestPtr request, callback_t callback)
{
	std::string login = request->getParameter("login");
	std::string email = request->getParameter("email");
	std::string password = request->getParameter("password");
	std::string password_confirm = request->getParameter("password-confirm");

	if (login.empty() || email.empty() || password.empty() || password_confirm.empty())
	{
		callback(gen_register_page(request->session(), drogon::HttpStatusCode::k400BadRequest, error::EMPTY_FIELDS));
		co_return;
	}
	if (password != password_confirm)
	{
		callback(gen_register_page(request->session(), drogon::HttpStatusCode::k400BadRequest, error::PASSWORDS_MISMATCH));
		co_return;
	}
	if (password.length() < 8)
	{
		callback(gen_register_page(request->session(), drogon::HttpStatusCode::k400BadRequest, error::TOO_SHORT_PW));
		co_return;
	}
	if (!data::is_login(login) || !data::is_email(email))
	{
		callback(gen_register_page(request->session(), drogon::HttpStatusCode::k400BadRequest, error::WRONG_FORMAT));
		co_return;
	}
	
	drogon::orm::DbClientPtr sql_client = drogon::app().getDbClient();
	data::user_t user = co_await get_user(sql_client, login);
	if (user.exist)
	{
		callback(gen_register_page(request->session(), drogon::HttpStatusCode::k400BadRequest, error::ALREADY_EXISTS));
		co_return;
	}

	auto now = std::chrono::system_clock::now().time_since_epoch();
	auto unix_now = std::chrono::duration_cast< std::chrono::seconds >(now);

	char hashed[crypto_pwhash_STRBYTES];
	if (crypto_pwhash_str(hashed, password.c_str(), password.length(),
				crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
	{
		callback(gen_register_page(request->session(), drogon::HttpStatusCode::k400BadRequest, error::UNACCEPTABLE_PW));
		co_return;
	}
	std::string enc_password(hashed);

	try
	{
		co_await sql_client->execSqlCoro("INSERT INTO users "
					"(NICKNAME, PASSWORD, EMAIL, LAST_ACTIVITY) VALUES (?, ?, ?, ?)",
					login, enc_password, email, unix_now.count());
	}
	catch (const drogon::orm::SqlError& e)
	{
		callback(gen_register_page(request->session(), drogon::HttpStatusCode::k400BadRequest, error::NOT_UNIQUE));
		co_return;
	}
	user = co_await get_user(sql_client, login);
	LOG_DEBUG << "User \"" << user.login << "\" (ID=" << user.id << ") registered";

	request->session()->erase("user");
	request->session()->insert("user", user);
	callback(default_redirect(user));
}
void account::controller::register_page(const drogon::HttpRequestPtr& request, callback_t&& callback)
{
	auto user = request->session()->getOptional< data::user_t >("user");
	if (user)
	{
		callback(default_redirect(*user));
	}
	else
	{
		callback(gen_register_page(request->session(), drogon::HttpStatusCode::k200OK, error::none));
	}
}
drogon::HttpResponsePtr account::controller::gen_register_page(const drogon::SessionPtr& session,
			drogon::HttpStatusCode code, error err)
{
	drogon::HttpViewData data;
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
	case error::ALREADY_EXISTS:
		data.insertAsString("error", "Пользователь с данным логином уже существует");
		break;
	case error::UNACCEPTABLE_PW:
		data.insertAsString("error", "Ошибка в обработке пароля - введите иной");
		break;
	case error::NOT_UNIQUE:
		data.insertAsString("error", "Пользователь с данной почтой уже существует");
		break;
	[[likely]]
	case error::none:
		break;
	default:
		data.insertAsString("error", "Неизвестная ошибка");
		break;
	}
	data.insert("sessid", session->sessionId());
	auto response = drogon::HttpResponse::newHttpViewResponse("views/account/register_p.csp", data);
	response->setStatusCode(code);
	return response;
}

drogon::Task<> account::controller::login(drogon::HttpRequestPtr request, callback_t callback)
{
	std::string login = request->getParameter("login");
	std::string password = request->getParameter("password");
	if (login.empty() || password.empty())
	{
		callback(gen_login_page(request->session(), drogon::HttpStatusCode::k400BadRequest, error::EMPTY_FIELDS));
		co_return;
	}
	if (!data::is_login(login))
	{
		callback(gen_login_page(request->session(), drogon::HttpStatusCode::k400BadRequest, error::WRONG_FORMAT));
		co_return;
	}

	auto now = std::chrono::system_clock::now().time_since_epoch();
	auto unix_now = std::chrono::duration_cast< std::chrono::seconds >(now);
	drogon::orm::DbClientPtr sql_client = drogon::app().getDbClient();
	drogon::orm::Result result = co_await sql_client->execSqlCoro("UPDATE users SET LAST_ACTIVITY = ?"
				" WHERE NICKNAME = ?", unix_now.count(), login);
	if (result.affectedRows() < 1)
	{
		callback(gen_login_page(request->session(), drogon::HttpStatusCode::k400BadRequest, error::FAILED));
		co_return;
	}
	data::user_t user = co_await get_user(sql_client, login);
	LOG_DEBUG << "User \"" << user.login << "\" (ID=" << user.id << ") is trying to log in";
	if (!check_password(user, password))
	{
		callback(gen_login_page(request->session(), drogon::HttpStatusCode::k400BadRequest, error::FAILED));
		co_return;
	}
	LOG_INFO << "User \"" << user.login << "\" (ID=" << user.id << ") loged in";
	request->session()->erase("user");
	request->session()->insert("user", user);
	callback(default_redirect(user));
}
void account::controller::login_page(const drogon::HttpRequestPtr& request, callback_t&& callback)
{
	auto user = request->session()->getOptional< data::user_t >("user");
	if (user)
	{
		callback(default_redirect(*user));
	}
	else
	{
		callback(gen_login_page(request->session(), drogon::HttpStatusCode::k200OK, error::none));
	}
}
drogon::HttpResponsePtr account::controller::gen_login_page(const drogon::SessionPtr& session,
			drogon::HttpStatusCode code, error err)
{
	drogon::HttpViewData data;
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
	[[likely]]
	case error::none:
		break;
	default:
		data.insertAsString("error", "Неизвестная ошибка");
		break;
	}
	data.insert("sessid", session->sessionId());
	auto response = drogon::HttpResponse::newHttpViewResponse("views/account/login.csp", data);
	response->setStatusCode(code);
	return response;
}

void account::controller::logout(const drogon::HttpRequestPtr& request, callback_t&& callback)
{
	request->session()->erase("user");
	callback(drogon::HttpResponse::newRedirectionResponse("/account/login.html"));
}
