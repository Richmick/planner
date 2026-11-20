#pragma once

#include <cstdint>
#include <exception>
#include <string_view>

#include <drogon/HttpController.h>
#include <http_utils.hpp>

namespace api::calendars
{
	class calendars_ctrl: public drogon::HttpController< calendars_ctrl >
	{
	public:
		static void initPathRouting();

		drogon::Task<> get_sources(api::request_t request, api::response_callback callback);
		drogon::Task<> create_source(api::request_t request, api::response_callback callback);
		drogon::Task<> get_calendars(api::request_t request, api::response_callback callback);
		drogon::Task<> create_calendar(api::request_t request, api::response_callback callback);
		drogon::Task<> get_events(api::request_t request, api::response_callback callback);
		drogon::Task<> get_active_events(api::request_t request, api::response_callback callback);

	private:
		struct responses
		{
			api::response_t k400_invalid_body;
			api::response_t k400_invalid_source_type;
			api::response_t k400_invalid_description;
			api::response_t k400_invalid_name;
			api::response_t k400_invalid_external_id;
			api::response_t k400_invalid_dates;
			api::response_t k401_unauthorized;
			api::response_t k404_not_found;
			api::response_t k500_database_error;
			api::response_t k502_source_error;

			responses();
		};
		responses resp;

		static std::int64_t parse_id(std::string_view value);
		static bool valid_source_type(std::string_view value);
		static Json::Value field_json(const drogon::orm::Row& row, const char* name);
		static drogon::Task< std::int64_t > find_ruz_group(const std::string& name);
		static api::response_t events_response(const drogon::orm::Result& rows);
		api::response_t database_error(const drogon::orm::DrogonDbException& error) const;
		api::response_t database_error(const std::exception& error) const;
	};
}
