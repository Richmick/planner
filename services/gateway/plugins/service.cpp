#include "proxying.hpp"

void api::gateway::service::proxy_callback::operator()(drogon::ReqResult result,
			const drogon::HttpResponsePtr& resp)
{
	if (result == drogon::ReqResult::Ok)
	{
		resp->setPassThrough(true);
		immediate_response(resp);
		return;
	}
	immediate_response(responses::instance().k502_bad_gateway);
}

bool api::gateway::service::is_up() const
{
	return exists();
}
bool api::gateway::service::exists() const
{
	return !address_.empty();
}
api::gateway::service::operator bool() const
{
	return exists();
}
void api::gateway::service::init(const Json::Value& config)
{
	if (config.isNull())
	{
		return;
	}
	if (!config.isObject())
	{
		throw std::runtime_error("service configuration static link must be object");
	}
	if (!config.isMember("address"))
	{
		throw std::runtime_error("no address in service static link configuration");
	}

	address_ = config.get("address", "").asString();
	pipelining_depth_ = config.get("pipelining", 0).asInt();
	same_client_to_same_backend_ = config.get("same_client_to_same_backend", false).asBool();
	connection_factor_ = config.get("connection_factor", 1).asInt();

	if (connection_factor_ == 0 || connection_factor_ > 100)
	{
		throw std::runtime_error("invalid number of connections per service");
	}
	client_index_.init([this](std::size_t& data, std::size_t thread_id) { data = thread_id; });
	connections_.init([this](std::vector< drogon::HttpClientPtr >& clients, std::size_t thread_id)
				{ clients.resize(connection_factor_); });
}

void api::gateway::service::redirect(const api::request_t& req, api::response_callback&& callback, bool as_is)
{
	if (!exists())
	{
		callback(responses::instance().k421_service_down);
		return;
	}
	req->setPassThrough(as_is);
	get(req)->sendRequest(req, proxy_callback{ std::move(callback) });
}
const drogon::HttpClientPtr& api::gateway::service::get(const api::request_t& request)
{
	std::size_t index;
	std::vector< drogon::HttpClientPtr >& conns = *connections_;
	if (same_client_to_same_backend_)
	{
		index = std::hash< std::uint32_t >{}(request->getPeerAddr().ipNetEndian()) % conns.size();
		index = (index + (++(*client_index_))) % conns.size();
	}
	else
	{
		index = ++(*client_index_) % conns.size();
	}
	auto& client_p = conns[index];
	if (!client_p)
	{
		client_p = drogon::HttpClient::newHttpClient(address_, trantor::EventLoop::getEventLoopOfCurrentThread());
		client_p->setPipeliningDepth(pipelining_depth_);
	}
	return client_p;
}
