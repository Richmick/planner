#pragma once

#include <cstdint>
#include <mutex>
#include <string>

#include <http_utils.hpp>

namespace api::calendars
{
	class calendar_notifier
	{
		std::once_flag client_initialized_;
		drogon::HttpClientPtr client_;

		void init_client();

	public:
		drogon::Task< api::response_t > send_access_request(
				std::int64_t calendar_id, std::int64_t user_id, std::string access);
	};
}
