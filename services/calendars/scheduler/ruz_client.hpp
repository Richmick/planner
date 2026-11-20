#pragma once

#include <chrono>

#include <drogon/drogon.h>

namespace api::calendars::ruz
{
	class client
	{
		using clock = std::chrono::steady_clock;

		drogon::HttpClientPtr http_;
		drogon::Mutex request_mutex_;
		clock::time_point next_request_;
		clock::time_point retry_after_;
		clock::duration interval_;
		clock::duration retry_interval_;
		double timeout_;

		drogon::Task< Json::Value > request(std::string path, std::string parameter, std::string value);

	public:
		client(const std::string& base_url, trantor::EventLoop* loop,
				std::chrono::duration< double > interval, double timeout,
				std::chrono::seconds retry_interval);

		drogon::Task< Json::Value > search_groups(std::string name);
		drogon::Task< Json::Value > fetch_week(std::int64_t group_id, std::chrono::local_days date);
	};
}
