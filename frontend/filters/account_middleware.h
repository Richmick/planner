#pragma once

#include <drogon/HttpMiddleware.h>

namespace api
{
	class logged_mw: public drogon::HttpMiddleware< logged_mw >
	{
	public:
		logged_mw() = default;

		void invoke(const drogon::HttpRequestPtr& request, drogon::MiddlewareNextCallback&& next_callback,
					drogon::MiddlewareCallback&& up_callback) override;
	};
	class admin_mw: public drogon::HttpMiddleware< admin_mw >
	{
	public:
		admin_mw() = default;

		void invoke(const drogon::HttpRequestPtr& request, drogon::MiddlewareNextCallback&& next_callback,
					drogon::MiddlewareCallback&& up_callback) override;
	};
}