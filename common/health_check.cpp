#include "health_check.hpp"

void api::plugins::health_check::initAndStart(const Json::Value& cfg)
{
	if (!cfg.get("url", Json::Value::nullSingleton()).isString())
	{
		LOG_FATAL << "Healthcheck config missing endpoint url";
		std::exit(2);
	}
	Json::String url = cfg.get("url", Json::Value::nullSingleton()).asString();
	PATH_ADD(url, drogon::Get);
	LOG_INFO << "Health check binded to " << url << ". Listening...";
}
void api::plugins::health_check::shutdown()
{
	LOG_INFO << "Health check shutdown";
}

void api::plugins::health_check::initPathRouting()
{}
void api::plugins::health_check::asyncHandleHttpRequest(const api::request_t& req, api::response_callback&& callback)
{
	api::response_t resp = api::response::new_response(code::k200_ok);
	if (req->getOptionalParameter< std::string >("load"))
	{
		unsigned cpu_usage = 0;
		std::int64_t mem_usage = 0;
		std::int64_t mem_available= 0;
		LOG_INFO << std::format("cpu: {}, memory: {}/{}", cpu_usage, mem_usage, mem_available);

		resp->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
		resp->setBody(std::format("{{\"time\":\"{}\",\"cpu\":{},\"mem\":{},\"avail-mem\":{}}}",
					utc_now(), cpu_usage, mem_usage, mem_available));
		return callback(resp);
	}
	resp->setBody(utc_now());
	callback(resp);
}
