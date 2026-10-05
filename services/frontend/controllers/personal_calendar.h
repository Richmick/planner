#pragma once

#include <drogon/HttpController.h>

namespace personal
{
	class calendar: public drogon::HttpController<calendar>
	{
	public:
		METHOD_LIST_BEGIN
			ADD_METHOD_TO(calendar::page, "/{:user_id}/calendar", drogon::Get);
			ADD_METHOD_TO(calendar::index, "/", drogon::Get);
		METHOD_LIST_END
		void page(const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback, std::size_t id);
		void index(const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback);
	};
}
