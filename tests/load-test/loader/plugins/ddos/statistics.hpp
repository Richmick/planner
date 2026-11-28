#pragma once

#include "area.hpp"

namespace tests::load
{
	struct statistics
	{
		using cell = std::uint_least64_t;
		struct resp_t
		{
			observable_area area = observable_area::last;

			clock::time_point start = clock::now();
			clock::time_point last_request;
			clock::time_point last_response;
			float_seconds durs{}, k2xx_durs{};

			cell pending = 0;
			cell recieved = 0;
			cell good = 0;

			cell net_error = 0;
			cell timeout = 0;
			cell k2xx = 0;
			cell k400 = 0;
			cell k401 = 0;
			cell k4xx = 0;
			cell k5xx = 0;

			std::vector< cell > qos_groups;

			void push(cell id, clock::time_point start,
						drogon::ReqResult res, const drogon::HttpResponsePtr& resp);
			resp_t& operator+=(const resp_t& rhs) noexcept;
		};
		struct auth_t
		{
			static constexpr inline observable_area area = observable_area::auth;
			cell inside = 0;
			cell logged = 0;
			cell refreshed = 0;
			cell registered = 0;
			cell expired_tokens = 0;
			cell unique_registrate_failed = 0;
			resp_t resp{area};
			auth_t& operator+=(const auth_t& rhs) noexcept;
		};
		struct calc_t
		{
			static constexpr inline observable_area area = observable_area::sched;
			resp_t resp{area};
			calc_t& operator+=(const calc_t& rhs) noexcept;
		};
		auth_t auth;
		calc_t calc;
		statistics& operator+=(const statistics& rhs) noexcept;
		resp_t& operator[](observable_area area) noexcept;
	};
	struct dynamic_status
	{
		enum class state_t
		{
			initializing, running, stopped
		};
		struct dynamic_area_frame
		{
			std::atomic< statistics::cell > sended = 0, recieved = 0, success = 0;
			statistics::cell max_queries = std::numeric_limits< statistics::cell >::max();
			float_seconds timeout{10};
			unsigned retries = 0;
			std::vector< float_seconds > qos_groups;
		};
		struct area_frame
		{
			statistics::cell delta_sended = 0, delta_recieved = 0, delta_success = 0;
		};
		struct frame
		{
			std::array< area_frame, n_areas > areas;
			std::size_t logged_users = 0, local_users = 0;
			clock::time_point now;
			statistics::cell start_id = 1;
		};
		using task_id = trantor::TimerId;

		std::array< dynamic_area_frame, n_areas > areas;
		std::atomic< std::size_t > local_users = 0, logged_users = 0;
		std::uniform_real_distribution< float_seconds::rep > wait_distr;
		float_seconds certain_time{1}, retry_429_timeout{30};
		std::size_t users_per_thread = 1, max_init_thread_queue = ~0UZ;

		std::deque< frame > graph;
		float_seconds frame_delay{5};
		frame commit_frame;
		clock::time_point graph_start;
		state_t state = state_t::stopped;
		task_id commit_task{};

		dynamic_area_frame& operator[](observable_area area);

		void set_qps(double user_qps);

		void send_routine(statistics::cell id, virtual_user& user, observable_area area); ///< Must be under shared lock
		void callback_routine(statistics::cell id, virtual_user& user, observable_area area,
					clock::time_point start, drogon::ReqResult res, const drogon::HttpResponsePtr& resp); ///< Must be under shared lock
	};
}
