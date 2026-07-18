#pragma once

#include "account_middleware.h"

#include <http_utils.h>
#include <data/user.h>

void api::logged_mw::invoke(const drogon::HttpRequestPtr& request,
			drogon::MiddlewareNextCallback&& next_callback, drogon::MiddlewareCallback&& up_callback)
{
	auto user = request->session()->getOptional< data::user_t >("user");
	if (!user)
	{
		api::send_error(std::move(up_callback), drogon::k401Unauthorized, "UNAUTHORIZED", "please do login");
		return;
	}
	next_callback(std::move(up_callback));
}
void api::admin_mw::invoke(const drogon::HttpRequestPtr& request,
			drogon::MiddlewareNextCallback&& next_callback, drogon::MiddlewareCallback&& up_callback)
{
	auto user = request->session()->getOptional< data::user_t >("user");
	if (!user)
	{
		api::send_error(std::move(up_callback), drogon::k401Unauthorized, "UNAUTHORIZED", "please do login");
		return;
	}
	if (!user->is_admin)
	{
		api::send_error(std::move(up_callback), drogon::k403Forbidden, "NOT_ADMIN", "requires admin");
		return;
	}
	next_callback(std::move(up_callback));
}
