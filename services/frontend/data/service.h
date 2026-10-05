#pragma once

#include <drogon/drogon.h>

namespace innerplane
{
	class service
	{
	public:
		void init(const Json::Value& config);
		drogon::HttpClientPtr get(const drogon::HttpRequestPtr& req);
	private:
		drogon::IOThreadStorage< std::vector< drogon::HttpClientPtr > > connections_;
		drogon::IOThreadStorage< std::size_t > client_index_{0};
		std::string address_;
		bool same_client_to_same_backend_{false};
		std::size_t connection_factor_{0};
		std::size_t pipelining_depth_{0};
	};
}
