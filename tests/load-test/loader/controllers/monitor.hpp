#pragma once

#include <drogon/HttpController.h>
#include <drogon/WebSocketController.h>

namespace tests::load
{
	class monitor: public drogon::HttpController< monitor >
	{
	public:
		static void initPathRouting();

		void start(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback);
		void stop(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback);
		void get_logs(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback, std::string service);
	};
	class socket: public drogon::WebSocketController< socket >
	{
	public:
		socket();

		static void initPathRouting();
		static socket& instance();

		void handleNewConnection(const drogon::HttpRequestPtr& req,
					const drogon::WebSocketConnectionPtr& conn) override;
		void handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) override;
		void handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
					std::string&& msg, const drogon::WebSocketMessageType& type);

		void broadcast(std::string_view msg);

	private:
		static socket* instance_;

		std::unordered_set< drogon::WebSocketConnectionPtr > listeners_;
		std::mutex mutex_;
	};
}
