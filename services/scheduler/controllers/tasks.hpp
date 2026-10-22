#pragma once

#include <string>
#include <drogon/HttpSimpleController.h>
#include <http_utils.hpp>

namespace api::scheduler
{
	struct event
	{
		using resolution = std::chrono::duration< std::int64_t, std::ratio< 60 > >;
		using time_point = std::chrono::sys_time< resolution >;

		std::int64_t id = 0;
		resolution duration;
		time_point start;
	};
	struct task
	{
		std::vector< event > events;
	};
	namespace ctrl
	{
		class tasks: public drogon::HttpSimpleController< tasks >
		{
		public:
			~tasks() noexcept = default;

			static void initPathRouting();
			void asyncHandleHttpRequest(const api::request_t& req, api::response_callback&& callback) override;

		private:
			struct responses
			{
			public:
				response_t k400_json_parse_error = api::response::new_response(api::code::k400_bad_request);
				response_t k400_not_json_array = api::response::new_response(api::code::k400_bad_request);
				response_t k400_event_missing_obligatory = api::response::new_response(api::code::k400_bad_request);
				response_t k400_event_fields_format = api::response::new_response(api::code::k400_bad_request);

				responses();
			};
			responses resp;
			std::atomic< std::uint64_t > task_id_ = 0;

			api::expected< task > parse_task(const api::request_t& req);
			api::expected< event > parse_event(const Json::Value& json);
			void schedule(task& tsk);
			response_t serialize_result(const task& tsk);
		};
	}
}
