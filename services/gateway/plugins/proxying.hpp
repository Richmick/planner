#pragma once

#include <optional>
#include <drogon/plugins/Plugin.h>
#include <drogon/drogon.h>
#include <http_utils.hpp>
#include "service.hpp"

namespace api::gateway
{
	class proxy_plugin;

	/// User profile data
	struct profile
	{
		std::int64_t id = 0;
		std::string name;
		std::string subscribes;
		bool is_admin = false;
	};

	/// Static responses set
	struct responses
	{
	public:
		response_t k204_ok = api::response::new_response(api::code::k204_no_content);

		response_t k401_unauthorized = api::response::new_401_unauthorized();
		response_t k403_forbidden = api::response::new_403_forbidden();
		response_t k404_not_found = api::response::new_404_not_found();
		response_t k419_invalid_csrf = api::response::new_response(api::code::k419_auth_timeout);
		response_t k421_service_down = api::response::new_response(api::code::k421_misdirected_request);
		response_t k421_frontend_down = api::response::new_response(api::code::k421_misdirected_request);

		response_t k500_internal = api::response::new_response(api::code::k500_internal);
		response_t k502_bad_gateway = api::response::new_response(api::code::k502_bad_gateway);
		response_t k504_timeout = api::response::new_response(api::code::k504_timeout);

		responses();
		static const responses& instance();

	private:
		static responses* instance_;
	};

	/// Main plugin
	class proxy_plugin: public drogon::Plugin< proxy_plugin >
	{
	public:
		service frontend;
		service users;
		service calendars;
		service scheduler;
		service notifier;

		/// Initialize service data (called when drogon::app().run())
		void initAndStart(const Json::Value& cfg) override;
		/// Exit handler
		void shutdown() override;
		/// Instance for accessing
		static proxy_plugin& instance();

	private:
		struct login_callback
		{
			api::response_callback callback;
			drogon::SessionPtr session;
			void operator()(drogon::ReqResult result, const api::response_t& resp);
		};
		static proxy_plugin* instance_;
		responses resp;

		/// Handler for preventing routes & proxing forward
		void pre_route(const drogon::HttpRequestPtr& req,
					drogon::AdviceCallback&& immediate_response,
					drogon::AdviceChainCallback&& chained_response);
	};
}
template<>
std::optional< api::gateway::profile > drogon::fromResponse(const drogon::HttpResponse& resp);
