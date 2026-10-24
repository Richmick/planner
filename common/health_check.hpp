#pragma once

#include <drogon/plugins/Plugin.h>
#include <drogon/HttpSimpleController.h>
#include <http_utils.hpp>

namespace api::plugins
{
	class health_check: public drogon::Plugin< health_check >, public drogon::HttpSimpleController< health_check, false >
	{
	public:
		/// Initialize service data (called when drogon::app().run())
		void initAndStart(const Json::Value& cfg) override;
		/// Exit handler
		void shutdown() override;

		/// Fictive method (body inside initAndStart instead)
		static void initPathRouting();
		/// Api endpoint
		void asyncHandleHttpRequest(const api::request_t& req, api::response_callback&& callback) override;
	};
}
