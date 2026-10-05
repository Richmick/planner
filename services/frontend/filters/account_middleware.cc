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
		up_callback(api::response::new_error(drogon::k401Unauthorized, "UNAUTHORIZED"_s, "please do login"_s));
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
		up_callback(api::response::new_error(drogon::k401Unauthorized, "UNAUTHORIZED"_s, "please do login"_s));
		return;
	}
	if (!user->is_admin)
	{
		up_callback(api::response::new_error(drogon::k403Forbidden, "NOT_ADMIN"_s, "requires admin"_s));
		return;
	}
	next_callback(std::move(up_callback));
}
