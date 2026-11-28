#pragma once

#include <chrono>
#include <drogon/drogon.h>

#undef max
#undef min

namespace tests::load
{
	using clock = std::chrono::steady_clock;
	using float_seconds = std::chrono::duration< double, std::ratio< 1 > >;

	enum class observable_area
	{
		auth, sched, last
	};
	constexpr inline std::size_t n_areas = static_cast< unsigned >(observable_area::last);
	constexpr std::string_view name_of(observable_area area);
	constexpr observable_area from_name(std::string_view area);

	struct virtual_user
	{
		struct login_data
		{
			std::string nickname;
			std::string password;
			std::string refresh_token;
		};
		enum state_t
		{
			auth_request,
			registrating,
			refreshing,
			logging,
			running,
			finished
		};
		drogon::HttpClientPtr client;
		login_data login;
		clock::time_point caret_time;
		std::int_least64_t local_id = 0, global_id = -1;
		unsigned retry = 0;
		state_t state = auth_request;

		void run(std::uint_least64_t id = 0);
		void switch_state(state_t new_state, std::shared_lock< std::shared_mutex >& lock);

		void schedule(std::uint_least64_t id);
		void authentificate(std::uint_least64_t id);
		void sched_cb(std::uint_least64_t id, clock::time_point start,
					drogon::ReqResult res, drogon::HttpResponsePtr resp);
		void auth_cb(std::uint_least64_t id, clock::time_point start,
					drogon::ReqResult res, drogon::HttpResponsePtr resp);
	};
}

constexpr std::string_view tests::load::name_of(observable_area area)
{
	switch (area)
	{
	case observable_area::auth:
		return "auth";
	case observable_area::sched:
		return "sched";
	default:
		std::unreachable();
	}
}
constexpr tests::load::observable_area tests::load::from_name(std::string_view area)
{
	if (area == "auth") return observable_area::auth;
	if (area == "sched") return observable_area::sched;
	return observable_area::last;
}
