#pragma once

#include <drogon/HttpController.h>
#include "api_restful_Events_ctrlBase.h"

#include "Events.h"


namespace api::restful
{
	using drogon_model::sqlite3::Events;

	class Events_ctrl:
		public drogon::HttpController< Events_ctrl >,
		public Events_ctrlBase
	{
	public:
		METHOD_LIST_BEGIN
			ADD_METHOD_TO(Events_ctrl::getOne, "/api/events/{1}", Get, Options);
			ADD_METHOD_TO(Events_ctrl::updateOne, "/api/events/{1}", Put, Options);
			ADD_METHOD_TO(Events_ctrl::deleteOne, "/api/events/{1}", Delete, Options);
			ADD_METHOD_TO(Events_ctrl::get, "/api/events", Get, Options);
			ADD_METHOD_TO(Events_ctrl::create, "/api/events", Post, Options);
			//ADD_METHOD_TO(Events_ctrl::update,"/api/events",Put,Options);
		METHOD_LIST_END

		void getOne(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Events::PrimaryKeyType&& id);
		void updateOne(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Events::PrimaryKeyType&& id);
		void deleteOne(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Events::PrimaryKeyType&& id);

		void get(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback);
		void create(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback);
	};
}
