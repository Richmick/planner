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
			ADD_METHOD_TO(Sources_ctrl::getOne,"/api/sources/{1}",Get,Options);
			ADD_METHOD_TO(Sources_ctrl::updateOne,"/api/sources/{1}",Put,Options);
			ADD_METHOD_TO(Sources_ctrl::deleteOne,"/api/sources/{1}",Delete,Options);
			ADD_METHOD_TO(Sources_ctrl::get,"/api/sources",Get,Options);
			ADD_METHOD_TO(Sources_ctrl::create,"/api/sources",Post,Options);
			//ADD_METHOD_TO(Sources_ctrl::update,"/api/sources",Put,Options);
		METHOD_LIST_END

		void getOne(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Sources::PrimaryKeyType&& id);
		void updateOne(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Sources::PrimaryKeyType&& id);
		void deleteOne(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Sources::PrimaryKeyType&& id);

		void get(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback);
		void create(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback);
	};
}
