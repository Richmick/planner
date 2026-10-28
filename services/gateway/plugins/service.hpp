#pragma once

#include <http_utils.hpp>

namespace api::gateway
{
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
}
