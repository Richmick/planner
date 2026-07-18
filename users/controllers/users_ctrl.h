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
			ADD_METHOD_TO(users_ctrl::getOne,"/api/users/{1}", drogon::Get, drogon::Options);
			ADD_METHOD_TO(users_ctrl::updateOne,"/api/users/{1}", drogon::Put, drogon::Options);
			ADD_METHOD_TO(users_ctrl::deleteOne,"/api/users/{1}", drogon::Delete, drogon::Options);
			ADD_METHOD_TO(users_ctrl::create,"/api/users", drogon::Post, drogon::Options);

			ADD_METHOD_TO(users_ctrl::login, "/api/login", drogon::Post, drogon::Options);
		METHOD_LIST_END

		void getOne(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Users::PrimaryKeyType&& id);
		void updateOne(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Users::PrimaryKeyType&& id);
		void deleteOne(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Users::PrimaryKeyType&& id);

		void create(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback);
		void login(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback);

		//bool subscribes_converter(const Json::Value& request, std::string& err);
	private:
		static long long now();
	};
}
