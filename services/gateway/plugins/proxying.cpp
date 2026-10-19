#include "proxying.hpp"

#include <string_view>
#include <http_utils.hpp>

using api::operator""_s;

api::gateway::proxy_plugin* api::gateway::proxy_plugin::instance_ = nullptr;

api::gateway::proxy_plugin& api::gateway::proxy_plugin::instance()
{
	return *instance_;
}
void api::gateway::proxy_plugin::initAndStart(const Json::Value& cfg)
{
	LOG_DEBUG << "Start parsing proxying plugin config";
	if (!cfg.isObject())
	{
		LOG_FATAL << "Invalid proxy config: expected object";
		std::abort();
	}
	Json::String route = cfg.get("route", "static").asString();
	if (route == "static")
	{
		const Json::Value links = cfg.get("links", Json::objectValue);
		for (const Json::String& service: links.getMemberNames())
		{
			try
			{
				if (service == "frontend") frontend.init(links[service]);
				else if (service == "users") users.init(links[service]);
				else if (service == "calendars") calendars.init(links[service]);
				else if (service == "scheduler") scheduler.init(links[service]);
				else if (service == "notifier") notifier.init(links[service]);
				else
				{
					LOG_FATAL << "Invalid proxy config: unknown service name: \"" << service << '"';
					std::abort();
				}
			}
			catch (const std::exception& e)
			{
				LOG_FATAL << "Invalid proxy config: " << e.what();
				std::abort();
			}
		}
		if (!users.exists())
		{
			LOG_FATAL << "Network missing obligatory service: \"users\"";
			std::abort();
		}
	}
	else
	{
		LOG_FATAL << "Invalid proxy config: unknown proxying policy: \"" << route << '"';
		std::abort();
	}
	LOG_DEBUG << "finish parsing proxying plugin config";

	instance_ = this;
	drogon::app().setCustom404Page(resp.k404_not_found);
	drogon::app().registerPreRoutingAdvice([this](const drogon::HttpRequestPtr& req,
					drogon::AdviceCallback&& immediate_response,
					drogon::AdviceChainCallback&& chained_response
				) { pre_route(req, std::move(immediate_response), std::move(chained_response)); });
	LOG_INFO << "Gateway proxy started. Listening...";
}
void api::gateway::proxy_plugin::shutdown()
{
	LOG_INFO << "Gateway proxy exited";
}

void api::gateway::proxy_plugin::pre_route(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& immediate_response,
			drogon::AdviceChainCallback&& chained_response)
{
	const std::string& path = req->getPath();
	if (!path.starts_with("/api"))
	{
		if (!frontend.exists())
			immediate_response(resp.k421_frontend_down);
		else
			frontend.redirect(req, std::move(immediate_response), true);
		return;
	}
	drogon::SessionPtr session = req->getSession();
	if (!session)
	{
		immediate_response(resp.k419_invalid_csrf);
		return;
	}

	std::string_view subpath{path.c_str() + 4, path.length() - 4};
	if (subpath.starts_with("/profiles")) // no logged required, no result analysis
	{
		users.redirect(req, std::move(immediate_response), true);
		return;
	}
	if (subpath.starts_with("/login")) // login and update session
	{
		req->setPassThrough(true);
		users.get(req)->sendRequest(req, login_callback{ std::move(immediate_response), std::move(session) });
		return;
	}
	std::optional user = session->getOptional< profile >("profile");
	if (!user)
	{
		immediate_response(resp.k401_unauthorized);
		return;
	}
	std::string content = req->getHeader("Content-Type");
	req->clearHeaders();
	req->addHeader("Content-Type", content);
	req->addHeader("user-id", std::to_string(user->id));
	req->addHeader("user-name", user->name);

	if (subpath.starts_with("/users/"))
		users.redirect(req, std::move(immediate_response));
	else if (subpath.starts_with("/calendars/"))
		calendars.redirect(req, std::move(immediate_response));
	else if (subpath.starts_with("/scheduler/"))
		scheduler.redirect(req, std::move(immediate_response));
	else if (subpath.starts_with("/notifier/"))
		notifier.redirect(req, std::move(immediate_response));
	else
		immediate_response(resp.k404_not_found);
}

void api::gateway::proxy_plugin::login_callback::operator()(drogon::ReqResult result, const api::response_t& resp)
{
	if (result == drogon::ReqResult::Timeout)
	{
		callback(responses::instance().k504_timeout);
	}
	else if (result != drogon::ReqResult::Ok)
	{
		callback(responses::instance().k502_bad_gateway);
	}
	else
	{
		if (api::code::ok(resp->getStatusCode()))
		{
			std::optional user = resp->as< std::optional< profile > >();
			if (!user) return callback(responses::instance().k500_internal);
			session->modify([&user](auto map){ map["profile"] = user; });
		}
		resp->setPassThrough(true);
		callback(resp);
	}
}
template<>
std::optional< api::gateway::profile > drogon::fromResponse(const drogon::HttpResponse& resp)
{
	const std::shared_ptr< Json::Value >& json = resp.jsonObject();
	if (!json || !json->isObject() || !json->isMember("id"))
	{
		return std::nullopt;
	}
	api::gateway::profile result;
	result.id = json->get("id", 0).asInt64();
	result.name = json->get("nickname", "").asString();
	return result;
}
