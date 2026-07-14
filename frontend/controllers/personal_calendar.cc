#include "personal_calendar.h"

#include <data/user.h>

void personal::calendar::page(const drogon::HttpRequestPtr& request, callback_t&& callback, std::size_t id)
{
	auto user = request->session()->getOptional< data::user_t >("user");
	drogon::HttpViewData data;
	if (user)
	{
		data.insert("sessid", request->session()->sessionId());
		if (user->id != id)
		{
			data.insertAsString("error", "Нет доступа к календарю данного пользователя");
		}
	}

	auto response = drogon::HttpResponse::newHttpViewResponse("views/calendar.csp", data);
	if (!user)
	{
		response->setStatusCode(drogon::HttpStatusCode::k401Unauthorized);
	}
	callback(response);
}
