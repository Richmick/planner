#pragma once

#include <drogon/HttpController.h>
#include "api_restful_Events_ctrlBase.h"

#include "Events.h"


namespace api::restful
{
	using drogon_model::sqlite3::Events;

	class events_ctrl:
		public drogon::HttpController< events_ctrl >,
		public Events_ctrlBase
	{
	public:
		METHOD_LIST_BEGIN
			ADD_METHOD_TO(events_ctrl::get_one, "/api/events/{1}", drogon::Get, drogon::Options);
			ADD_METHOD_TO(events_ctrl::update_one, "/api/events/{1}", drogon::Put, drogon::Options);
			ADD_METHOD_TO(events_ctrl::delete_one, "/api/events/{1}", drogon::Delete, drogon::Options);
			ADD_METHOD_TO(events_ctrl::get, "/api/events", drogon::Get, drogon::Options);
			ADD_METHOD_TO(events_ctrl::create, "/api/events", drogon::Post, drogon::Options);
		METHOD_LIST_END

		void get_one(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Events::PrimaryKeyType&& id);
		void update_one(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Events::PrimaryKeyType&& id);
		void delete_one(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
					Events::PrimaryKeyType&& id);

		void get(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback);
		void create(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback);

	private:
		using base_type = Events_ctrlBase;
	};
}
