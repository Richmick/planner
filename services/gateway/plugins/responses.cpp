#include "proxying.hpp"

api::gateway::responses* api::gateway::responses::instance_ = nullptr;

api::gateway::responses::responses()
{
	instance_ = this;
	k419_invalid_csrf->setBody("{\"error\":\"Update session\"}");
	k421_service_down->setBody("{\"error\":\"Service is disabled\"}");
	k421_frontend_down->setBody("Frontend is disabled. Only api calls accepted");
	k500_internal->setBody("{\"error\":\"Internal service error\"}");
	k502_bad_gateway->setBody("{\"error\":\"Service is down\"}");
	k504_timeout->setBody("{\"error\":\"Service response timeout exceeded\"}");
}
const api::gateway::responses& api::gateway::responses::instance()
{
	return *instance_;
}
