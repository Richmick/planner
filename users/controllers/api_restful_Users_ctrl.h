#pragma once

#include <drogon/HttpController.h>
#include "api_restful_Users_ctrlBase.h"

#include "Users.h"

using drogon_model::sqlite3::Users;

namespace api::restful
{
	class Users_ctrl:
		public drogon::HttpController< Users_ctrl >,
		public Users_ctrlBase
	{
	public:
		METHOD_LIST_BEGIN
			ADD_METHOD_TO(Users_ctrl::getOne,"/api/users/{1}",Get,Options);
			ADD_METHOD_TO(Users_ctrl::updateOne,"/api/users/{1}",Put,Options);
			ADD_METHOD_TO(Users_ctrl::deleteOne,"/api/users/{1}",Delete,Options);
			ADD_METHOD_TO(Users_ctrl::get,"/api/users",Get,Options);
			ADD_METHOD_TO(Users_ctrl::create,"/api/users",Post,Options);
			ADD_METHOD_TO(Users_ctrl::login, "/api/login", Post);
		METHOD_LIST_END

		void getOne(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Users::PrimaryKeyType&& id);
		void updateOne(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Users::PrimaryKeyType&& id);
		void deleteOne(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Users::PrimaryKeyType&& id);

		void get(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback);
		void create(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback);
		void login(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback);
	};
}
