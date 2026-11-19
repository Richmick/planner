#include "calendars_ctrl.h"
#include "../scheduler/ruz_scheduler.hpp"

#include <drogon/HttpClient.h>

#include <charconv>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <memory>
#include <regex>
#include <string>
#include <utility>

namespace
{
    using api::calendars::calendars_ctrl;
    using callback_ptr = std::shared_ptr<drogon::AdviceCallback>;

    callback_ptr share_callback(drogon::AdviceCallback&& callback)
    {
        return std::make_shared<drogon::AdviceCallback>(std::move(callback));
    }

    std::int64_t request_user_id(const drogon::HttpRequestPtr& request)
    {
        const std::string value = request->getHeader("user-id");
        if (value.empty()) return 0;
        std::int64_t parsed = 0;
        const char* first = value.data();
        const char* last = first + value.size();
        const auto result = std::from_chars(first, last, parsed);
        if (result.ec != std::errc{} || result.ptr != last || parsed <= 0) return 0;
        return parsed;
    }

    bool valid_identifier(const std::string& value)
    {
        static const std::regex pattern("^[a-zA-Z0-9_-]{1,64}$");
        return std::regex_match(value, pattern);
    }

    bool valid_date(const std::string& value)
    {
        static const std::regex pattern("^[0-9]{4}-[0-9]{2}-[0-9]{2}$");
        if (!std::regex_match(value, pattern)) return false;
        const auto year = std::chrono::year{std::stoi(value.substr(0, 4))};
        const auto month = std::chrono::month{static_cast<unsigned>(std::stoi(value.substr(5, 2)))};
        const auto day = std::chrono::day{static_cast<unsigned>(std::stoi(value.substr(8, 2)))};
        return std::chrono::year_month_day{year, month, day}.ok();
    }

    Json::Value parse_json_value(const drogon::orm::Row& row, const char* name)
    {
        if (row[name].isNull()) return Json::Value(Json::nullValue);
        const std::string value = row[name].as<std::string>();
        Json::Value result;
        Json::CharReaderBuilder builder;
        std::string error;
        std::istringstream input(value);
        if (!Json::parseFromStream(builder, input, &result, &error)) return Json::Value(value);
        return result;
    }

    Json::Value event_json(const drogon::orm::Row& row)
    {
        Json::Value event;
        event["event_id"] = row["event_id"].as<std::int64_t>();
        event["calendar_id"] = row["calendar_id"].as<std::int64_t>();
        event["source_type"] = row["source_type"].as<std::string>();
        event["name"] = row["name"].as<std::string>();
        event["scheduled"] = parse_json_value(row, "effective_scheduled");
        event["confirmed"] = parse_json_value(row, "confirmed");
        event["finished"] = row["is_finished"].as<bool>();
        event["success"] = row["is_success"].as<bool>();
        event["dependencies"] = parse_json_value(row, "effective_dependencies");
        event["restrictions"] = parse_json_value(row, "effective_restrictions");
        event["overlap"] = row["overlap"].as<std::string>();
        event["interruptible"] = row["interruptible"].as<bool>();
        event["version"] = row["version"].as<int>();
        return event;
    }

    void database_error(const callback_ptr& callback, const drogon::orm::DrogonDbException& error)
    {
        LOG_ERROR << "Calendars database error: " << error.base().what();
        (*callback)(calendars_ctrl::k500_database_error);
    }

    void save_calendar(
        const std::string& source_type,
        const std::string& external_id,
        const std::string& name,
        std::int64_t user_id,
        const callback_ptr& callback
    )
    {
        Json::Value metadata;
        metadata["name"] = name;
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        const std::string metadata_json = Json::writeString(builder, metadata);

        drogon::app().getDbClient()->execSqlAsync(
            "WITH saved_calendar AS ("
            " INSERT INTO calendars (type_id, external_id, metadata) VALUES ($1, $2, $3::jsonb)"
            " ON CONFLICT (type_id, external_id) DO UPDATE SET metadata = EXCLUDED.metadata"
            " RETURNING calendar_id"
            "), saved_subscription AS ("
            " INSERT INTO subscriptions (user_id, calendar_id)"
            " SELECT $4, calendar_id FROM saved_calendar"
            " ON CONFLICT (user_id, calendar_id) DO NOTHING"
            " RETURNING calendar_id"
            ")"
            " SELECT calendar_id FROM saved_calendar",
            [callback, source_type, external_id, name](const drogon::orm::Result& rows)
            {
                if (rows.empty())
                {
                    (*callback)(calendars_ctrl::k404_not_found);
                    return;
                }
                const std::int64_t calendar_id = rows.front()["calendar_id"].as<std::int64_t>();
                if (source_type == "ruz") api::calendars::sync_ruz_calendar(calendar_id, external_id);
                Json::Value response;
                response["calendar_id"] = calendar_id;
                response["source_type"] = source_type;
                response["external_id"] = external_id;
                response["name"] = name;
                (*callback)(api::response::new_json(std::move(response), drogon::k201Created));
            },
            [callback](const drogon::orm::DrogonDbException& error)
            {
                database_error(callback, error);
            },
            source_type, external_id, metadata_json, user_id
        );
    }

    void create_ruz_calendar(
        const std::string& group_name,
        std::int64_t user_id,
        const callback_ptr& callback
    )
    {
        auto client = drogon::HttpClient::newHttpClient("https://ruz.spbstu.ru");
        auto request = drogon::HttpRequest::newHttpRequest();
        request->setMethod(drogon::Get);
        request->setPath("/api/v1/ruz/search/groups");
        request->setParameter("q", group_name);
        client->sendRequest(request,
            [client, group_name, user_id, callback](drogon::ReqResult result,
                    const drogon::HttpResponsePtr& response)
            {
                if (result != drogon::ReqResult::Ok || !response
                    || response->getStatusCode() != drogon::k200OK)
                {
                    (*callback)(calendars_ctrl::k502_source_error);
                    return;
                }

                const auto json = response->jsonObject();
                if (!json || !(*json)["groups"].isArray())
                {
                    (*callback)(calendars_ctrl::k502_source_error);
                    return;
                }

                std::string group_id;
                for (const Json::Value& group : (*json)["groups"])
                {
                    if (group.get("name", "").asString() == group_name)
                    {
                        const std::int64_t id = group.get("id", 0).asInt64();
                        if (id > 0) group_id = std::to_string(id);
                        break;
                    }
                }
                if (group_id.empty())
                {
                    (*callback)(calendars_ctrl::k404_not_found);
                    return;
                }
                save_calendar("ruz", group_id, group_name, user_id, callback);
            }
        );
    }

    void get_events_for_user(
        std::int64_t user_id,
        const std::string& date_from,
        const std::string& date_to,
        bool active_only,
        const callback_ptr& callback
    )
    {
        std::string sql =
            "SELECT e.event_id, e.calendar_id, c.type_id AS source_type, e.name, "
            "COALESCE(o.local_scheduled, e.global_scheduled) AS effective_scheduled, "
            "o.confirmed, COALESCE(o.finished, e.finished, FALSE) AS is_finished, "
            "COALESCE(o.success, FALSE) AS is_success, "
            "COALESCE(o.dependencies, e.base_dependencies) AS effective_dependencies, "
            "COALESCE(o.restrictions, e.base_restrictions) AS effective_restrictions, "
            "COALESCE(e.overlap, 'inclusive') AS overlap, "
            "COALESCE(e.interruptible, TRUE) AS interruptible, COALESCE(e.version, 1) AS version "
            "FROM events e "
            "JOIN calendars c ON c.calendar_id = e.calendar_id "
            "JOIN subscriptions s ON s.calendar_id = e.calendar_id "
            "LEFT JOIN event_overrides o ON o.event_id = e.event_id AND o.user_id = s.user_id "
            "WHERE s.user_id = $1 ";

        if (active_only)
        {
            sql +=
                "AND COALESCE(o.finished, e.finished, FALSE) = FALSE "
                "AND COALESCE(o.local_scheduled, e.global_scheduled, '[]'::jsonb) <> '[]'::jsonb ";
        }
        else
        {
            sql +=
                "AND EXISTS ("
                " SELECT 1 FROM jsonb_array_elements(COALESCE(o.local_scheduled, e.global_scheduled, '[]'::jsonb)) slot "
                " WHERE (slot->>'start')::timestamptz < (($3::date + INTERVAL '1 day')::timestamp AT TIME ZONE 'Europe/Moscow') "
                " AND (slot->>'end')::timestamptz >= ($2::date::timestamp AT TIME ZONE 'Europe/Moscow')"
                ") ";
        }

        sql +=
            "ORDER BY (COALESCE(o.local_scheduled, e.global_scheduled)->0->>'start')::timestamptz, e.event_id";

        auto db = drogon::app().getDbClient();
        if (active_only)
        {
            db->execSqlAsync(sql,
                [callback](const drogon::orm::Result& rows)
                {
                    Json::Value events(Json::arrayValue);
                    for (const auto& row : rows) events.append(event_json(row));
                    (*callback)(api::response::new_json(std::move(events)));
                },
                [callback](const drogon::orm::DrogonDbException& error) { database_error(callback, error); },
                user_id
            );
        }
        else
        {
            db->execSqlAsync(sql,
                [callback](const drogon::orm::Result& rows)
                {
                    Json::Value events(Json::arrayValue);
                    for (const auto& row : rows) events.append(event_json(row));
                    (*callback)(api::response::new_json(std::move(events)));
                },
                [callback](const drogon::orm::DrogonDbException& error) { database_error(callback, error); },
                user_id, date_from, date_to
            );
        }
    }
}

const api::response_t api::calendars::calendars_ctrl::k400_bad_request =
    api::response::new_400("BAD_REQUEST", "Invalid request body or query parameters");
const api::response_t api::calendars::calendars_ctrl::k401_unauthorized =
    api::response::new_401_unauthorized();
const api::response_t api::calendars::calendars_ctrl::k404_not_found =
    api::response::new_404_not_found();
const api::response_t api::calendars::calendars_ctrl::k500_database_error =
    api::response::new_error(drogon::k500InternalServerError, "DB_ERROR", "Internal database error");
const api::response_t api::calendars::calendars_ctrl::k502_source_error =
    api::response::new_error(drogon::k502BadGateway, "SOURCE_UNAVAILABLE", "RUZ service is unavailable");
void api::calendars::calendars_ctrl::get_sources(
    const drogon::HttpRequestPtr&, drogon::AdviceCallback&& callback)
{
    auto cb = share_callback(std::move(callback));
    drogon::app().getDbClient()->execSqlAsync(
        "SELECT type_id, COALESCE(description, '') AS description FROM calendar_types ORDER BY type_id",
        [cb](const drogon::orm::Result& rows)
        {
            Json::Value response(Json::arrayValue);
            for (const auto& row : rows)
            {
                Json::Value source;
                source["type_id"] = row["type_id"].as<std::string>();
                source["description"] = row["description"].as<std::string>();
                response.append(std::move(source));
            }
            (*cb)(api::response::new_json(std::move(response)));
        },
        [cb](const drogon::orm::DrogonDbException& error) { database_error(cb, error); }
    );
}

void api::calendars::calendars_ctrl::create_source(
    const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback)
{
    auto cb = share_callback(std::move(callback));
    const auto body = request->getJsonObject();
    if (!body || !body->isObject()) return (*cb)(k400_bad_request);

    const std::string type_id = body->get("type_id", "").asString();
    const std::string description = body->get("description", "").asString();
    if (!valid_identifier(type_id)) return (*cb)(k400_bad_request);

    drogon::app().getDbClient()->execSqlAsync(
        "INSERT INTO calendar_types (type_id, description) VALUES ($1, $2) "
        "ON CONFLICT (type_id) DO UPDATE SET description = EXCLUDED.description "
        "RETURNING type_id, description",
        [cb](const drogon::orm::Result& rows)
        {
            Json::Value source;
            source["type_id"] = rows.front()["type_id"].as<std::string>();
            source["description"] = rows.front()["description"].as<std::string>();
            (*cb)(api::response::new_json(std::move(source), drogon::k201Created));
        },
        [cb](const drogon::orm::DrogonDbException& error) { database_error(cb, error); },
        type_id, description
    );
}

void api::calendars::calendars_ctrl::get_calendars(
    const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback)
{
    auto cb = share_callback(std::move(callback));
    const std::int64_t user_id = request_user_id(request);
    if (user_id <= 0) return (*cb)(k401_unauthorized);

    drogon::app().getDbClient()->execSqlAsync(
        "SELECT c.calendar_id, c.type_id, c.external_id, c.metadata, COALESCE(c.created_at, NOW()) AS created_at "
        "FROM calendars c JOIN subscriptions s ON s.calendar_id = c.calendar_id "
        "WHERE s.user_id = $1 ORDER BY c.created_at DESC, c.calendar_id DESC",
        [cb](const drogon::orm::Result& rows)
        {
            Json::Value calendars(Json::arrayValue);
            for (const auto& row : rows)
            {
                Json::Value calendar;
                calendar["calendar_id"] = row["calendar_id"].as<std::int64_t>();
                calendar["source_type"] = row["type_id"].as<std::string>();
                calendar["external_id"] = row["external_id"].as<std::string>();
                calendar["metadata"] = parse_json_value(row, "metadata");
                calendar["created_at"] = row["created_at"].as<std::string>();
                calendars.append(std::move(calendar));
            }
            (*cb)(api::response::new_json(std::move(calendars)));
        },
        [cb](const drogon::orm::DrogonDbException& error) { database_error(cb, error); },
        user_id
    );
}

void api::calendars::calendars_ctrl::create_calendar(
    const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback)
{
    auto cb = share_callback(std::move(callback));
    const std::int64_t user_id = request_user_id(request);
    if (user_id <= 0) return (*cb)(k401_unauthorized);

    const auto body = request->getJsonObject();
    if (!body || !body->isObject()) return (*cb)(k400_bad_request);

    const std::string source_type = body->get("source_type", "ruz").asString();
    const std::string name = body->get("name", body->get("group", "")).asString();
    const std::string external_id = body->get("external_id", "").asString();
    if (!valid_identifier(source_type) || name.empty()) return (*cb)(k400_bad_request);

    if (source_type == "ruz")
    {
        create_ruz_calendar(name, user_id, cb);
        return;
    }
    if (external_id.empty()) return (*cb)(k400_bad_request);
    save_calendar(source_type, external_id, name, user_id, cb);
}

void api::calendars::calendars_ctrl::get_events(
    const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback)
{
    auto cb = share_callback(std::move(callback));
    const std::int64_t user_id = request_user_id(request);
    if (user_id <= 0) return (*cb)(k401_unauthorized);

    std::string date_from = request->getParameter("from");
    std::string date_to = request->getParameter("to");
    if (date_from.empty()) date_from = request->getParameter("date_from");
    if (date_to.empty()) date_to = request->getParameter("date_to");
    if (!valid_date(date_from) || !valid_date(date_to) || date_from > date_to)
        return (*cb)(k400_bad_request);

    get_events_for_user(user_id, date_from, date_to, false, cb);
}

void api::calendars::calendars_ctrl::get_active_events(
    const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback)
{
    auto cb = share_callback(std::move(callback));
    const std::int64_t user_id = request_user_id(request);
    if (user_id <= 0) return (*cb)(k401_unauthorized);
    get_events_for_user(user_id, {}, {}, true, cb);
}
