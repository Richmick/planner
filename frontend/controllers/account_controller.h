#pragma once

#include <drogon/HttpController.h>
#include <drogon/utils/coroutine.h>

namespace account
{
	class controller: public drogon::HttpController< controller >
	{
	public:
		METHOD_LIST_BEGIN
			ADD_METHOD_TO(controller::login_page, "/account/login", drogon::Get);
			ADD_METHOD_TO(controller::login, "/account/login", drogon::Post);
			ADD_METHOD_TO(controller::register_page, "/account/register", drogon::Get);
			ADD_METHOD_TO(controller::register_user, "/account/register", drogon::Post);
			ADD_METHOD_TO(controller::logout, "/account/logout", drogon::Post);

			ADD_METHOD_TO(controller::pass_self, "/api/users/{:id}/subscribe", drogon::Post,
						drogon::Options, "api::logged_mw");
			ADD_METHOD_TO(controller::pass_self, "/api/users/{:id}", drogon::Get, drogon::Put,
						drogon::Options, "api::logged_mw");
			ADD_METHOD_TO(controller::pass_self, "/api/users/{:id}", drogon::Delete, "api::admin_mw");

			ADD_METHOD_TO(controller::pass, "/api/users", drogon::Get, "api::admin_mw");
			ADD_METHOD_TO(controller::pass_login, "/api/users", drogon::Post, drogon::Options);
			ADD_METHOD_TO(controller::pass_login, "/api/login", drogon::Post, drogon::Options);
		METHOD_LIST_END

		drogon::Task<> register_user(drogon::HttpRequestPtr request, drogon::AdviceCallback callback);
		drogon::Task<> login(drogon::HttpRequestPtr request, drogon::AdviceCallback callback);
		void logout(const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback);
		void login_page(const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback);
		void register_page(const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback);

		drogon::Task<> pass(drogon::HttpRequestPtr request, drogon::AdviceCallback callback);
		drogon::Task<> pass_login(drogon::HttpRequestPtr request, drogon::AdviceCallback callback);
		drogon::Task<> pass_self(drogon::HttpRequestPtr request, drogon::AdviceCallback callback,
					std::size_t target_id);

	private:
		enum class error
		{
			none,
			EMPTY_FIELDS,
			WRONG_FORMAT,
			FAILED,
			ALREADY_EXISTS,
			TOO_SHORT_PW,
			PASSWORDS_MISMATCH,
			UNACCEPTABLE_PW,
			INTERNAL
		};
		error parse_user(const drogon::SessionPtr& session,
					const drogon::HttpResponsePtr& response, std::size_t& user_id);
		drogon::HttpResponsePtr gen_login_page(const drogon::SessionPtr& session, error err);
		drogon::HttpResponsePtr gen_register_page(const drogon::SessionPtr& session, error err);
	};
}
