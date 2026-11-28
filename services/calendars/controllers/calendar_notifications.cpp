#include "calendar_notifications.hpp"

#include <stdexcept>

namespace api::calendars
{
	void calendar_notifier::init_client()
	{
		const auto& config = drogon::app().getCustomConfig()["notifier"];
		std::string address = config.get("address", "").asString();
		if (address.empty()) return;

		if (address.ends_with('/')) address.pop_back();
		const bool https = address.starts_with("https://");
		const std::size_t scheme_length = https ? 8 : 7;
		if ((!https && !address.starts_with("http://")) || address.size() <= scheme_length
					|| address.find_first_of("/?#@ \t\r\n", scheme_length) != std::string::npos)
			throw std::invalid_argument("Expected an HTTP(S) origin for notifier.address");

		client_ = drogon::HttpClient::newHttpClient(address, drogon::app().getLoop());
	}

	drogon::Task< api::response_t > calendar_notifier::send_access_request(
			std::int64_t calendar_id, std::int64_t user_id, std::string access)
	{
		try
		{
			std::call_once(client_initialized_, [this] { init_client(); });
			if (!client_)
				co_return api::response::new_error(drogon::k421MisdirectedRequest,
						"NOTIFIER_DISABLED", "Notifier service is disabled");

			Json::Value body;
			body["calendar_id"] = calendar_id;
			body["user_id"] = user_id;
			body["event"] = "access-request";
			body["access"] = std::move(access);
			auto request = drogon::HttpRequest::newHttpJsonRequest(body);
			request->setMethod(drogon::Post);
			request->setPath("/api/notifier/events");

			const auto response = co_await client_->sendRequestCoro(std::move(request), 5.0);
			if (response && api::code::ok(response->getStatusCode()))
				co_return api::response::new_response(drogon::k202Accepted);
			if (response) LOG_WARN << "Notifier access request returned HTTP " << response->getStatusCode();
		}
		catch (const drogon::HttpException& error)
		{
			LOG_WARN << "Notifier access request failed: " << error.what();
			if (error.code() == drogon::ReqResult::Timeout)
				co_return api::response::new_error(drogon::k504GatewayTimeout,
						"NOTIFIER_TIMEOUT", "Notifier service did not respond in time");
		}
		catch (const std::exception& error)
		{
			LOG_ERROR << "Notifier access request failed: " << error.what();
		}
		co_return api::response::new_error(drogon::k502BadGateway, "NOTIFIER_UNAVAILABLE",
				"Notifier service did not accept the access request");
	}
}
