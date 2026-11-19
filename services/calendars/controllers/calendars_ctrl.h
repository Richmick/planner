#pragma once

#include <drogon/HttpController.h>
#include <http_utils.hpp>

namespace api::calendars
{
    class calendars_ctrl : public drogon::HttpController<calendars_ctrl>
    {
    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(calendars_ctrl::get_sources, "/api/calendars/sources", drogon::Get, drogon::Options);
            ADD_METHOD_TO(calendars_ctrl::create_source, "/api/calendars/sources", drogon::Post, drogon::Options);
            ADD_METHOD_TO(calendars_ctrl::get_calendars, "/api/calendars/calendars", drogon::Get, drogon::Options);
            ADD_METHOD_TO(calendars_ctrl::create_calendar, "/api/calendars/calendars", drogon::Post, drogon::Options);
            ADD_METHOD_TO(calendars_ctrl::get_events, "/api/calendars/events", drogon::Get, drogon::Options);
            ADD_METHOD_TO(calendars_ctrl::get_active_events, "/api/calendars/events/active", drogon::Get, drogon::Options);
        METHOD_LIST_END

        void get_sources(const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback);
        void create_source(const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback);
        void get_calendars(const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback);
        void create_calendar(const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback);
        void get_events(const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback);
        void get_active_events(const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback);

        static const api::response_t k400_bad_request;
        static const api::response_t k401_unauthorized;
        static const api::response_t k404_not_found;
        static const api::response_t k500_database_error;
        static const api::response_t k502_source_error;
    };
}
