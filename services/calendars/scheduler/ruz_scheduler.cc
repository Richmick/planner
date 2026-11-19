#include "ruz_scheduler.hpp"

#include <drogon/drogon.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <ctime>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_set>

namespace
{
    using namespace std::chrono_literals;

    std::string current_date()
    {
        const std::time_t now = std::time(nullptr);
        std::tm value{};
#ifdef _WIN32
        localtime_s(&value, &now);
#else
        localtime_r(&now, &value);
#endif
        std::ostringstream result;
        result << std::put_time(&value, "%Y-%m-%d");
        return result.str();
    }

    std::time_t to_local_time(const std::string& date, const std::string& time)
    {
        std::tm value{};
        std::istringstream input(date + " " + time);
        input >> std::get_time(&value, "%Y-%m-%d %H:%M");
        if (input.fail()) return 0;
        value.tm_isdst = -1;
        return std::mktime(&value);
    }

    std::string json_string(const Json::Value& value)
    {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        return Json::writeString(builder, value);
    }

    std::string normalize_time(std::string value)
    {
        if (value.size() == 4 && value[1] == ':') value.insert(value.begin(), '0');
        if (value.size() == 5) value += ":00";
        return value;
    }

    std::string lesson_key(const std::string& date, const Json::Value& lesson)
    {
        const std::string value = date + "|"
            + lesson.get("time_start", "").asString() + "|"
            + lesson.get("time_end", "").asString() + "|"
            + lesson.get("subject", "").asString();
        std::ostringstream result;
        result << std::hex << std::hash<std::string>{}(value);
        return result.str();
    }

    class ruz_scheduler
    {
    public:
        void start()
        {
            const Json::Value config = drogon::app().getCustomConfig().get("ruz", Json::objectValue);
            base_url_ = config.get("base_url", "https://ruz.spbstu.ru").asString();
            daily_sync_hour_ = config.get("daily_sync_hour", 5).asInt();
            refresh_before_minutes_ = config.get("refresh_before_minutes", 30).asInt();
            if (daily_sync_hour_ < 0 || daily_sync_hour_ > 23) daily_sync_hour_ = 5;
            if (refresh_before_minutes_ < 0 || refresh_before_minutes_ > 1440) refresh_before_minutes_ = 30;
            schedule_daily_sync();
            sync_all();
        }

        void sync_calendar(std::int64_t calendar_id, std::string group_id)
        {
            fetch_schedule(calendar_id, std::move(group_id), current_date(), true);
        }

    private:
        void schedule_daily_sync()
        {
            const std::time_t now = std::time(nullptr);
            std::tm target{};
#ifdef _WIN32
            localtime_s(&target, &now);
#else
            localtime_r(&now, &target);
#endif
            target.tm_hour = daily_sync_hour_;
            target.tm_min = 0;
            target.tm_sec = 0;
            std::time_t next = std::mktime(&target);
            if (next <= now)
            {
                target.tm_mday += 1;
                next = std::mktime(&target);
            }
            const double delay = std::difftime(next, now);
            drogon::app().getLoop()->runAfter(delay, [this]
            {
                sync_all();
                schedule_daily_sync();
            });
        }

        void sync_all()
        {
            drogon::app().getDbClient()->execSqlAsync(
                "SELECT calendar_id, external_id FROM calendars WHERE type_id = 'ruz' ORDER BY calendar_id",
                [this](const drogon::orm::Result& rows)
                {
                    const std::string date = current_date();
                    for (const auto& row : rows)
                    {
                        fetch_schedule(
                            row["calendar_id"].as<std::int64_t>(),
                            row["external_id"].as<std::string>(),
                            date,
                            true
                        );
                    }
                },
                [](const drogon::orm::DrogonDbException& error)
                {
                    LOG_ERROR << "RUZ schedule list query failed: " << error.base().what();
                }
            );
        }

        void fetch_schedule(
            std::int64_t calendar_id,
            std::string group_id,
            std::string date,
            bool schedule_refreshes
        )
        {
            auto client = drogon::HttpClient::newHttpClient(base_url_);
            auto request = drogon::HttpRequest::newHttpRequest();
            request->setMethod(drogon::Get);
            request->setPath("/api/v1/ruz/scheduler/" + group_id);
            request->setParameter("date", date);
            client->sendRequest(request,
                [this, client, calendar_id, group_id = std::move(group_id), date = std::move(date), schedule_refreshes]
                (drogon::ReqResult result, const drogon::HttpResponsePtr& response)
                {
                    if (result != drogon::ReqResult::Ok || !response
                        || response->getStatusCode() != drogon::k200OK)
                    {
                        LOG_WARN << "RUZ schedule request failed for group " << group_id;
                        return;
                    }

                    const auto data = response->jsonObject();
                    if (!data || !data->isObject() || !(*data)["days"].isArray())
                    {
                        LOG_WARN << "RUZ returned an invalid schedule for group " << group_id;
                        return;
                    }

                    const std::time_t now = std::time(nullptr);
                    for (const Json::Value& day : (*data)["days"])
                    {
                        const std::string lesson_date = day.get("date", "").asString();
                        if (!day["lessons"].isArray()) continue;

                        for (const Json::Value& lesson : day["lessons"])
                        {
                            const std::string start_time = lesson.get("time_start", "").asString();
                            const std::string end_time = lesson.get("time_end", "").asString();
                            const std::string subject = lesson.get("subject", "").asString();
                            if (lesson_date.empty() || start_time.empty() || end_time.empty() || subject.empty())
                                continue;

                            const std::string start = lesson_date + "T" + normalize_time(start_time) + "+03:00";
                            const std::string end = lesson_date + "T" + normalize_time(end_time) + "+03:00";
                            Json::Value scheduled(Json::arrayValue);
                            Json::Value interval;
                            interval["start"] = start;
                            interval["end"] = end;
                            scheduled.append(std::move(interval));

                            Json::Value source_data;
                            source_data["date"] = lesson_date;
                            source_data["lesson"] = lesson;
                            const std::string external_id = lesson_key(lesson_date, lesson);
                            const std::string scheduled_json = json_string(scheduled);
                            const std::string source_json = json_string(source_data);

                            drogon::app().getDbClient()->execSqlAsync(
                                "INSERT INTO events (calendar_id, external_id, name, global_scheduled, source_data, finish_before) "
                                "VALUES ($1, $2, $3, $4::jsonb, $5::jsonb, $6::timestamptz) "
                                "ON CONFLICT (calendar_id, external_id) DO UPDATE SET "
                                "name = EXCLUDED.name, global_scheduled = EXCLUDED.global_scheduled, "
                                "source_data = EXCLUDED.source_data, finish_before = EXCLUDED.finish_before, "
                                "updated_at = NOW(), version = events.version + 1",
                                [](const drogon::orm::Result&) {},
                                [group_id, lesson_date](const drogon::orm::DrogonDbException& error)
                                {
                                    LOG_ERROR << "RUZ event update failed for group " << group_id
                                        << " on " << lesson_date << ": " << error.base().what();
                                },
                                calendar_id, external_id, subject, scheduled_json, source_json, end
                            );

                            if (!schedule_refreshes) continue;
                            const std::time_t starts_at = to_local_time(lesson_date, start_time);
                            if (starts_at <= now) continue;
                            const std::time_t refresh_at = starts_at - refresh_before_minutes_ * 60;
                            const std::string refresh_key = std::to_string(calendar_id) + "|" + lesson_date
                                + "|" + start_time;
                            {
                                std::lock_guard lock(refresh_mutex_);
                                if (!scheduled_refreshes_.insert(refresh_key).second) continue;
                            }
                            const double delay = std::max(1.0, std::difftime(refresh_at, now));
                            drogon::app().getLoop()->runAfter(delay,
                                [this, calendar_id, group_id, lesson_date, refresh_key]
                                {
                                    {
                                        std::lock_guard lock(refresh_mutex_);
                                        scheduled_refreshes_.erase(refresh_key);
                                    }
                                    fetch_schedule(calendar_id, group_id, lesson_date, false);
                                }
                            );
                        }
                    }
                }
            );
        }

        std::string base_url_ = "https://ruz.spbstu.ru";
        int daily_sync_hour_ = 5;
        int refresh_before_minutes_ = 30;
        std::mutex refresh_mutex_;
        std::unordered_set<std::string> scheduled_refreshes_;
    };

    ruz_scheduler scheduler;
}

void api::calendars::start_ruz_scheduler()
{
    scheduler.start();
}

void api::calendars::sync_ruz_calendar(std::int64_t calendar_id, std::string group_id)
{
    scheduler.sync_calendar(calendar_id, std::move(group_id));
}
