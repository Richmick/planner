#pragma once

#include <drogon/HttpController.h>
#include "api_restful_Sources_ctrlBase.h"

#include "Sources.h"

using drogon_model::sqlite3::Sources;

namespace api::restful
{
	class Sources_ctrl:
		public drogon::HttpController< Sources_ctrl >,
		public Sources_ctrlBase
	{
	public:
		METHOD_LIST_BEGIN
			ADD_METHOD_TO(Sources_ctrl::get_one,"/api/sources/{1}", drogon::Get, drogon::Options);
			ADD_METHOD_TO(Sources_ctrl::update_one,"/api/sources/{1}", drogon::Put, drogon::Options);
			ADD_METHOD_TO(Sources_ctrl::delete_one,"/api/sources/{1}", drogon::Delete, drogon::Options);
			ADD_METHOD_TO(Sources_ctrl::subscribe_user,"/api/sources/renew-subscribes/{1:user-id}",
						drogon::Post, drogon::Options);
			ADD_METHOD_TO(Sources_ctrl::get,"/api/sources", drogon::Get, drogon::Options);
			ADD_METHOD_TO(Sources_ctrl::create,"/api/sources", drogon::Post, drogon::Options);
		METHOD_LIST_END

		void get_one(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Sources::PrimaryKeyType&& id);
		void update_one(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Sources::PrimaryKeyType&& id);
		void delete_one(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Sources::PrimaryKeyType&& id);

		void subscribe_user(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Sources::PrimaryKeyType&& id);
		void get(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback);
		void create(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback);

	private:
		using base_type = Sources_ctrlBase;
	};
}
