#include "ruz_client.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>

namespace api::calendars::ruz
{
	namespace
	{
		struct request_error: std::runtime_error
		{
			bool temporary;
			request_error(std::string message, bool retry): std::runtime_error(message), temporary(retry) {}
		};

		bool temporary_failure(drogon::ReqResult code)
		{
			return code == drogon::ReqResult::Timeout || code == drogon::ReqResult::NetworkFailure
					|| code == drogon::ReqResult::BadResponse;
		}

		Json::Value read_response(const drogon::HttpResponsePtr& response)
		{
			if (!response)
				throw request_error("empty response", true);
			const int status = response->statusCode();
			if (status != drogon::k200OK)
				throw request_error(std::format("HTTP {}", status), status >= 500 || status == 408 || status == 429);

			const auto& json = response->getJsonObject();
			if (!json)
				throw request_error("invalid JSON: " + response->getJsonError(), false);
			return *json;
		}
	}

	client::client(const std::string& base_url, trantor::EventLoop* loop,
			std::chrono::duration< double > interval, double timeout, std::chrono::seconds retry_interval)
		: http_(drogon::HttpClient::newHttpClient(base_url, loop)),
		  interval_(std::chrono::duration_cast< clock::duration >(interval)),
		  retry_interval_(retry_interval), timeout_(timeout)
	{
	}

	drogon::Task< Json::Value > client::search_groups(std::string name)
	{
		co_return co_await request("/api/v1/ruz/search/groups", "q", std::move(name));
	}

	drogon::Task< Json::Value > client::fetch_week(std::int64_t group_id, std::chrono::local_days date)
	{
		co_return co_await request(std::format("/api/v1/ruz/scheduler/{}", group_id), "date",
				std::format("{:%F}", std::chrono::year_month_day{date}));
	}

	drogon::Task< Json::Value > client::request(std::string path, std::string parameter, std::string value)
	{
		auto* loop = http_->getLoop();
		co_await drogon::switchThreadCoro(loop);
		auto lock = co_await request_mutex_.scoped_lock(loop);
		if (clock::now() < retry_after_)
			throw std::runtime_error("RUZ is temporarily unavailable");

		for (int attempt = 0; ; ++attempt)
		{
			const auto delay = next_request_ - clock::now();
			if (delay > clock::duration::zero())
				co_await drogon::sleepCoro(loop, std::chrono::duration< double >{delay});
			if (!drogon::app().isRunning())
				throw std::runtime_error("calendar service is stopping");

			auto request = drogon::HttpRequest::newHttpRequest();
			request->setMethod(drogon::Get);
			request->setPath(path);
			request->setParameter(parameter, value);
			request->addHeader("Accept", "application/json");
			next_request_ = clock::now() + interval_;
			try
			{
				co_return read_response(co_await http_->sendRequestCoro(request, timeout_));
			}
			catch (const drogon::HttpException& error)
			{
				LOG_WARN << "RUZ " << path << ": " << error.what();
				if (!temporary_failure(error.code()))
					throw;
			}
			catch (const request_error& error)
			{
				LOG_WARN << "RUZ " << path << ": " << error.what();
				if (!error.temporary)
					throw;
			}

			next_request_ = clock::now() + (std::max)(interval_, retry_interval_);
			if (attempt == 0)
				continue;
			retry_after_ = next_request_;
			throw std::runtime_error("RUZ request failed after retry");
		}
	}
}
