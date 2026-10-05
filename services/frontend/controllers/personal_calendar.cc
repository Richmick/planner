#include "personal_calendar.h"

#include <data/user.h>

void personal::calendar::page(const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback,
			std::size_t id)
{
	auto user = request->session()->getOptional< data::user_t >("user");
	drogon::HttpViewData data;
	data.insert("calendar_id", id);
	if (user)
	{
		data.insert("sessid", request->session()->sessionId());
		if (user->id != id)
		{
			data.insertAsString("error", "Нет доступа к запрошенному календарю");
			data.insertAsString("calendar_name", "Календарь #" + std::to_string(id));
		}
		else
		{
			data.insertAsString("calendar_name", "Календарь пользователя " + user->login);
		}
	}
	else
	{
		data.insertAsString("error", "Необходима авторизация");
		data.insertAsString("calendar_name", "Календарь #" + std::to_string(id));
	}

	auto response = drogon::HttpResponse::newHttpViewResponse("views/calendar.csp", data);
	if (!user)
	{
		response->setStatusCode(drogon::HttpStatusCode::k401Unauthorized);
	}
	else if (user->id != id)
	{
		response->setStatusCode(drogon::HttpStatusCode::k403Forbidden);
	}
	callback(response);
}
void personal::calendar::index(const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback)
{
	auto user = request->session()->getOptional< data::user_t >("user");
	drogon::HttpViewData data;
	if (user)
	{
		data.insert("sessid", request->session()->sessionId());
	}
	auto response = drogon::HttpResponse::newHttpViewResponse("views/index.csp", data);
	callback(response);
}
