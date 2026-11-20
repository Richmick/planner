#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include <drogon/drogon.h>

#include "database_utils.hpp"

namespace api::calendars
{
	class outbox_relay final : public drogon::Plugin< outbox_relay >
	{
	private:
		struct relay_state
		{
			drogon::HttpClientPtr client;
			drogon::orm::DbClientPtr database;
			std::string path;
			std::string select_pending;
			std::string delete_delivered;
			std::atomic_bool running{false};
			std::atomic_bool stopped{false};
		};

		std::shared_ptr< relay_state > state_;
		trantor::TimerId timer_{};

		static drogon::Task< bool > deliver(const relay_state& state, const std::string& id,
				const std::string& payload)
		{
			Json::Value body(Json::objectValue);
			std::istringstream input(payload);
			Json::CharReaderBuilder reader;
			std::string error;
			if (!Json::parseFromStream(reader, input, &body["payload"], &error))
			{
				LOG_ERROR << "Invalid outbox payload " << id << ": " << error;
				co_return false;
			}
			body["id"] = id;

			auto request = drogon::HttpRequest::newHttpJsonRequest(body);
			request->setMethod(drogon::Post);
			request->setPath(state.path);
			request->setPathEncode(false);
			const auto response = co_await state.client->sendRequestCoro(request, 10);
			const auto status = response->getStatusCode();
			if (status < 200 || status >= 300)
			{
				LOG_WARN << "Outbox delivery " << id << " returned HTTP " << status;
				co_return false;
			}
			co_return true;
		}

		static drogon::AsyncTask flush(std::shared_ptr< relay_state > state)
		{
			if (state->stopped || state->running.exchange(true)) co_return;
			try
			{
				while (!state->stopped)
				{
					const auto rows = co_await state->database->execSqlCoro(state->select_pending);
					if (rows.empty()) break;

					bool delivered = false;
					for (const auto& row: rows)
					{
						if (state->stopped) break;
						const auto id = row["id"].as< std::string >();
						delivered = co_await deliver(*state, id, row["payload"].as< std::string >());
						if (!delivered) break;
						co_await state->database->execSqlCoro(state->delete_delivered, id);
					}
					if (!delivered) break;
				}
			}
			catch (const drogon::orm::DrogonDbException& error)
			{
				LOG_ERROR << "Outbox database error: " << error.base().what();
			}
			catch (const std::exception& error)
			{
				LOG_WARN << "Outbox delivery failed: " << error.what();
			}
			state->running = false;
		}

	public:
		void initAndStart(const Json::Value& config) override
		{
			const auto url = config.get("url", "");
			const auto interval = config.get("retry_minutes", 5);
			if (!url.isString() || !interval.isNumeric() || !std::isfinite(interval.asDouble())
						|| interval.asDouble() <= 0)
			{
				throw std::invalid_argument("Invalid outbox url or retry_minutes");
			}

			const auto endpoint = url.asString();
			if (endpoint.empty())
			{
				LOG_INFO << "Outbox delivery awaits a notifier URL";
				return;
			}
			const std::size_t host_start = endpoint.starts_with("https://") ? 8 : 7;
			const auto path_start = endpoint.find('/', host_start);
			if ((!endpoint.starts_with("http://") && !endpoint.starts_with("https://"))
						|| path_start == host_start || endpoint.size() <= host_start
						|| endpoint.find_first_of("?#@ \t\r\n") != std::string::npos)
			{
				throw std::invalid_argument("Expected an HTTP(S) notifier URL");
			}

			auto* loop = drogon::app().getLoop();
			state_ = std::make_shared< relay_state >();
			state_->client = drogon::HttpClient::newHttpClient(endpoint.substr(0, path_start), loop);
			state_->database = drogon::app().getDbClient();
			state_->path = path_start == std::string::npos ? "/" : endpoint.substr(path_start);
			state_->select_pending = db::is_postgresql()
				? "SELECT id, payload FROM outbox_events ORDER BY id LIMIT 100"
				: "SELECT id, payload FROM outbox_events ORDER BY rowid LIMIT 100";
			state_->delete_delivered = db::query("DELETE FROM outbox_events WHERE id = ?");
			const auto retry_delay = std::chrono::duration< double, std::ratio< 60 > >(interval.asDouble());
			timer_ = loop->runEvery(retry_delay, [state = state_] { flush(state); });
			loop->queueInLoop([state = state_] { flush(state); });
		}

		void shutdown() override
		{
			if (!state_) return;
			state_->stopped = true;
			drogon::app().getLoop()->invalidateTimer(timer_);
			state_.reset();
		}
	};
}
