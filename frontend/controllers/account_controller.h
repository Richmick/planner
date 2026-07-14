#pragma once

#include <drogon/HttpController.h>
#include <drogon/utils/coroutine.h>

namespace account
{
	class controller: public drogon::HttpController< controller >
	{
	private:
		using callback_t = std::function< void(const drogon::HttpResponsePtr&) >;

		enum class error
		{
			none,
			EMPTY_FIELDS,
			WRONG_FORMAT,
			FAILED,
			NOT_UNIQUE,
			ALREADY_EXISTS,
			TOO_SHORT_PW,
			PASSWORDS_MISMATCH,
			UNACCEPTABLE_PW
		};
	public:
		METHOD_LIST_BEGIN
			ADD_METHOD_TO(controller::login_page, "/account/login", drogon::Get);
			ADD_METHOD_TO(controller::login, "/account/login", drogon::Post);
			ADD_METHOD_TO(controller::register_page, "/account/register", drogon::Get);
			ADD_METHOD_TO(controller::register_user, "/account/register", drogon::Post);
			ADD_METHOD_TO(controller::logout, "/account/logout", drogon::Post);
		METHOD_LIST_END

		drogon::Task<> register_user(drogon::HttpRequestPtr request, callback_t callback);
		drogon::Task<> login(drogon::HttpRequestPtr request, callback_t callback);
		void logout(const drogon::HttpRequestPtr& request, callback_t&& callback);
		void login_page(const drogon::HttpRequestPtr& request, callback_t&& callback);
		void register_page(const drogon::HttpRequestPtr& request, callback_t&& callback);
		drogon::HttpResponsePtr gen_login_page(const drogon::SessionPtr& session,
					drogon::HttpStatusCode code, error err);
		drogon::HttpResponsePtr gen_register_page(const drogon::SessionPtr& session,
					drogon::HttpStatusCode code, error err);
	};
}
