#pragma once

#include <chrono>
#include <random>
#include <drogon/drogon.h>

namespace tests::load
{
	using clock = std::chrono::steady_clock;
	using float_seconds = std::chrono::duration< double, std::ratio< 1 > >;

	class ddos_plugin;

	struct virtual_user
	{
		enum state_t
		{
			init, registrating, logged, finished
		};
		drogon::HttpClientPtr client;
		clock::time_point caret_time;
		state_t state = init;
		std::uint_least64_t local_id = 0, global_id = 0;
		void send_request(std::uint_least64_t id = 0);
		void req_callback(std::uint_least64_t id, clock::time_point start,
					drogon::ReqResult res, drogon::HttpResponsePtr resp);
	};
	struct statistics
	{
		using cell = std::uint_least64_t;
		struct resp_t
		{
			clock::time_point start = clock::now();
			clock::time_point last_request;
			clock::time_point last_response;
			float_seconds durs{}, k2xx_durs{};

			cell pending = 0;
			cell good = 0;
			cell bad = 0;

			cell net_error = 0;
			cell timeout = 0;
			cell k2xx = 0;
			cell k400 = 0;
			cell k401 = 0;
			cell k4xx = 0;
			cell k5xx = 0;

			void push(cell id, clock::time_point start,
						drogon::ReqResult res, const drogon::HttpResponsePtr& resp);
			resp_t& operator+=(const resp_t& rhs) noexcept;
		};
		struct auth_t
		{
			cell logged = 0;
			cell expired_tokens = 0;
			cell unique_registrate_failed = 0;
			resp_t resp;
			auth_t& operator+=(const auth_t& rhs) noexcept;
		};
		struct calc_t
		{
			resp_t resp;
			calc_t& operator+=(const calc_t& rhs) noexcept;
		};
		auth_t auth;
		calc_t calc;
		statistics& operator+=(const statistics& rhs) noexcept;
	};
	struct thread_data
	{
		std::mt19937_64 engine;
		statistics stats;
		std::vector< virtual_user > users;
	};

	class ddos_plugin: public drogon::Plugin< ddos_plugin >
	{
	public:
		float_seconds certain_time{1}, timeout{10};
		std::uint_least64_t max_queries = 10;
		std::size_t users_per_thread = 1;
		std::uniform_real_distribution< float_seconds::rep > wait_distr;
		drogon::IOThreadStorage< thread_data > threads;
		std::atomic< std::uint_least64_t > sended = 0, success = 0, errors = 0;

		/// Initialize service data (called when drogon::app().run())
		void initAndStart(const Json::Value& cfg) override;
		/// Exit handler
		void shutdown() override;
		/// Instance for accessing
		static ddos_plugin& instance();

	private:
		using task_id = trantor::TimerId;

		static ddos_plugin* instance_;
		clock::time_point start_, last_;
		std::uint_least64_t last_sended_ = 0;

		std::string gateway_;

		/// Exception throwing init
		void safe_init(const Json::Value& cfg);

		void print_status();
		void thread_init(thread_data& gen, std::uint64_t seed, std::size_t thread_id);
	};
}
