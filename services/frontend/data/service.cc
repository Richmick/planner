#include "service.h"

void innerplane::service::init(const Json::Value& config)
{
	if (!config.isMember("address"))
	{
		LOG_ERROR << "no address in service configuration";
		abort();
	}

	address_ = config.get("address", "").asString();
	pipelining_depth_ = config.get("pipelining", 0).asInt();
	same_client_to_same_backend_ = config.get("same_client_to_same_backend", false).asBool();
	connection_factor_ = config.get("connection_factor", 1).asInt();

	if (connection_factor_ == 0 || connection_factor_ > 100)
	{
		LOG_ERROR << "invalid number of connection factor per service";
		abort();
	}
	client_index_.init([this](std::size_t& data, std::size_t thread_id) { data = thread_id; });
	connections_.init([this](std::vector< drogon::HttpClientPtr >& clients, std::size_t thread_id)
				{ clients.resize(connection_factor_); });
}

drogon::HttpClientPtr innerplane::service::get(const drogon::HttpRequestPtr& request)
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
