#include "OutboxWorker.h"
#include <drogon/drogon.h>
#include <thread>
#include <chrono>
#include <string>

using namespace drogon;

void OutboxWorker::run() {
    auto db = app().getDbClient();
    auto config = app().getCustomConfig();
    std::string users_url = config["net"]["users"]["address"].asString();
    auto client = HttpClient::newHttpClient(users_url);

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        try {
            auto trans = db->newTransaction();
            auto result = trans->execSqlSync(
                "SELECT id, aggregate_id, event_type, payload FROM outbox_events "
                "WHERE status = 'pending' ORDER BY created_at ASC LIMIT 10 FOR UPDATE SKIP LOCKED"
            );

            for (const auto& row : result) {
                std::string id = row["id"].as<std::string>();
                std::string agg_id = row["aggregate_id"].as<std::string>();
                std::string event_type = row["event_type"].as<std::string>();
                std::string payload = row["payload"].as<std::string>();

                Json::Value payload_json;
                Json::Reader reader;
                reader.parse(payload, payload_json);
                std::string user_id = std::to_string(payload_json["user_id"].asUInt64());

                auto req = HttpRequest::newHttpRequest();
                req->setMethod(Post);
                req->setPath("/api/users/" + user_id + "/outbox");
                req->setBody(payload);

                auto [res, resp] = client->sendRequest(req, 5.0);
                
                if (resp && resp->statusCode() >= 200 && resp->statusCode() < 300) {
                    trans->execSqlSync("UPDATE outbox_events SET status = 'processed', processed_at = NOW() WHERE id = $1", id);
                } else {
                    trans->execSqlSync(
                        "UPDATE outbox_events SET retry_count = retry_count + 1, "
                        "status = CASE WHEN retry_count >= 5 THEN 'failed' ELSE 'pending' END "
                        "WHERE id = $1", id
                    );
                }
            }
            trans->commit();
        } catch (const std::exception& e) {
            LOG_ERROR << "Outbox worker error: " << e.what();
        }
    }
}
