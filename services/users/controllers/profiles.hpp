#pragma once

#include <string>
#include <drogon/HttpController.h>
#include <http_utils.hpp>

#include <models/users.hpp>

namespace api::users
{
	static constexpr inline std::size_t pw_min_length = 8;

	struct login_data
	{
		std::string nickname;
		std::string password;
	};
	struct register_data
	{
		std::string nickname;
		std::string email;
		std::string password;
	};

	namespace ctrl
	{
		class profiles: public drogon::HttpController< profiles >
		{
		public:
			static void initPathRouting();

			drogon::Task<> get_one(api::request_t req, api::response_callback callback, id_t id);
			drogon::Task<> update_one(api::request_t req, api::response_callback callback, id_t id);
			drogon::Task<> delete_one(api::request_t req, api::response_callback callback, id_t id);

			drogon::Task<> subscribe(api::request_t req, api::response_callback callback, id_t id);
			drogon::Task<> create_one(api::request_t req, api::response_callback callback);
			drogon::Task<> login(api::request_t req, api::response_callback callback);

		private:
			struct responses
			{
			public:
				response_t k204_ok = api::response::new_response(api::code::k204_no_content);

				response_t k400_not_json = api::response::new_response(api::code::k400_bad_request);
				response_t k400_missing_obligatory_login = api::response::new_response(api::code::k400_bad_request);
				response_t k400_missing_obligatory_register = api::response::new_response(api::code::k400_bad_request);
				response_t k400_failed_auth = api::response::new_response(api::code::k400_bad_request);
				response_t k400_nickname_busy = api::response::new_response(api::code::k400_bad_request);
				response_t k400_email_busy = api::response::new_response(api::code::k400_bad_request);
				response_t k400_password_too_short = api::response::new_response(api::code::k400_bad_request);
				response_t k400_unsupported_password = api::response::new_response(api::code::k400_bad_request);

				response_t k500_db_error = api::response::new_response(api::code::k500_internal);
				
				responses();
			};
			responses resp;

			api::expected< login_data > parse_login(const api::request_t& req);
			api::expected< register_data > parse_register(const api::request_t& req);
		};
	}
}
