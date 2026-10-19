#pragma once

#include <optional>
#include <drogon/plugins/Plugin.h>
#include <drogon/drogon.h>
#include <http_utils.hpp>

namespace api::gateway
{
	class proxy_plugin;

	/// Service proxy
	class service
	{
	public:
		struct proxy_callback
		{
			drogon::AdviceCallback immediate_response;
			void operator()(drogon::ReqResult result, const drogon::HttpResponsePtr& resp);
		};

		/// Real time status
		bool is_up() const;
		/// Configuration time status
		bool exists() const;
		/// Same as exists()
		operator bool() const;

		/// Initialize with config (if present)
		void init(const Json::Value& config);
		/// Proxy request with default handler (proxy back to user or send k502_bad_gateway)
		void redirect(const api::request_t& req, api::response_callback&& callback, bool as_is = false);

		/// Determine client
		const drogon::HttpClientPtr& get(const api::request_t& req);

	private:
		drogon::IOThreadStorage< std::vector< drogon::HttpClientPtr > > connections_;
		drogon::IOThreadStorage< std::size_t > client_index_{0};
		std::string address_;
		std::size_t connection_factor_{0};
		std::size_t pipelining_depth_{0};
		bool same_client_to_same_backend_{false};
	};

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
		response_t k401_unauthorized = api::response::new_response(api::code::k401_unauthorized);
		response_t k403_forbidden = api::response::new_response(api::code::k403_forbidden);
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
