#pragma once

#include <drogon/HttpController.h>

#include "Users.h"

using drogon_model::sqlite3::Users;

namespace api::restful
{
	class users_ctrl: public drogon::HttpController< users_ctrl >
	{
	public:
		METHOD_LIST_BEGIN
			ADD_METHOD_TO(users_ctrl::get_one,"/api/users/{1}", drogon::Get, drogon::Options);
			ADD_METHOD_TO(users_ctrl::update_one,"/api/users/{1}", drogon::Put, drogon::Options);
			ADD_METHOD_TO(users_ctrl::delete_one,"/api/users/{1}", drogon::Delete, drogon::Options);
			ADD_METHOD_TO(users_ctrl::subscribe, "/api/users/{1}/subscribe", drogon::Post, drogon::Options);
			ADD_METHOD_TO(users_ctrl::create,"/api/users", drogon::Post, drogon::Options);

			ADD_METHOD_TO(users_ctrl::login, "/api/login", drogon::Post, drogon::Options);
		METHOD_LIST_END

		void get_one(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Users::PrimaryKeyType&& id);
		void update_one(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Users::PrimaryKeyType&& id);
		void delete_one(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Users::PrimaryKeyType&& id);

		void subscribe(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Users::PrimaryKeyType&& id);
		void create(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback);
		void login(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback);

	private:
		static long long now();
	};
}
