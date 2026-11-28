#include "monitor.hpp"
#include <plugins/ddos.hpp>

void tests::load::monitor::initPathRouting()
{
	ADD_METHOD_TO(monitor::start, "/load-test/start", drogon::Post, drogon::Options);
	ADD_METHOD_TO(monitor::stop, "/load-test/stop", drogon::Post, drogon::Options);
	ADD_METHOD_TO(monitor::get_logs, "/load-test/logs/{1:service}", drogon::Get, drogon::Options);
}
void tests::load::monitor::get_logs(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback, std::string service)
{
	std::filesystem::path* path = nullptr;
	if (service == "gateway")
		path = &ddos_plugin::instance().gateway_logs;
	else if (service == "gateway-access")
		path = &ddos_plugin::instance().gateway_access_logs;
	else if (service == "users")
		path = &ddos_plugin::instance().users_logs;
	else if (service == "users-access")
		path = &ddos_plugin::instance().users_access_logs;
	/*else if (service == "scheduler")
	else if (service == "scheduler-access")*/
	else
	{
		callback(drogon::HttpResponse::newNotFoundResponse());
		return;
	}
	std::filesystem::directory_entry res;
	if (path->empty())
	{
		drogon::HttpResponsePtr resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::k500InternalServerError);
		resp->setBody("Log file unreachable");
		callback(resp);
		return;
	}
	for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator{path->parent_path()})
	{
		if (!entry.is_regular_file()) continue;
		if (entry.path().filename().string().starts_with(path->filename().string()))
		{
			if (res.path().empty() || (res.last_write_time() < entry.last_write_time()))
			{
				res = entry;
			}
		}
	}
	if (res.path().empty())
	{
		drogon::HttpResponsePtr resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::k500InternalServerError);
		resp->setBody("Log file not found");
		callback(resp);
		return;
	}
	callback(drogon::HttpResponse::newFileResponse(res.path().string(), service + ".log", drogon::CT_TEXT_PLAIN));
}
void tests::load::monitor::start(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{

}
void tests::load::monitor::stop(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
	ddos_plugin::instance().stop();
}

tests::load::socket* tests::load::socket::instance_ = nullptr;

tests::load::socket::socket()
{
	instance_ = this;
}
void tests::load::socket::initPathRouting()
{
	WS_PATH_ADD("/load-test/status");
}
tests::load::socket& tests::load::socket::instance()
{
	return *instance_;
}
void tests::load::socket::handleNewConnection(const drogon::HttpRequestPtr& req,
			const drogon::WebSocketConnectionPtr& conn)
{
	std::unique_lock lock{mutex_};
	listeners_.emplace(conn);
}
void tests::load::socket::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn)
{
	std::unique_lock lock{mutex_};
	listeners_.erase(conn);
}
void tests::load::socket::handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
			std::string&& msg, const drogon::WebSocketMessageType& type)
{}
void tests::load::socket::broadcast(std::string_view msg)
{
	std::unique_lock lock{mutex_};
	for (const drogon::WebSocketConnectionPtr& conn: listeners_)
	{
		conn->send(msg);
	}
}
