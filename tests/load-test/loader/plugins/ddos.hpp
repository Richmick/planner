#pragma once

#include <random>
#include <shared_mutex>
#include <fstream>
#include <drogon/drogon.h>
#include "ddos/area.hpp"
#include "ddos/statistics.hpp"

namespace tests::load
{
	struct thread_data
	{
		std::mt19937_64 engine;
		statistics stats;
		std::vector< virtual_user > users;
	};

	class ddos_plugin: public drogon::Plugin< ddos_plugin >
	{
	public:
		drogon::IOThreadStorage< thread_data > threads;
		dynamic_status status;
		std::shared_mutex rw_mutex;
		std::atomic< statistics::cell > id_generator;

		std::filesystem::path gateway_logs, gateway_access_logs, users_logs, users_access_logs;
		std::vector< std::string > sched_logs, sched_access_logs;

		/// Initialize service data (called when drogon::app().run())
		void initAndStart(const Json::Value& cfg) override;
		/// Exit handler
		void shutdown() override;
		/// Instance for accessing
		static ddos_plugin& instance();

		void start();
		void escalate();
		void stop();

	private:
		static ddos_plugin* instance_;

		std::ifstream logins_file_;
		std::filesystem::path logins_filename_;

		std::string gateway_;

		/// Exception throwing init
		void safe_init(const Json::Value& cfg);

		void print_status();
		std::string serialize_area_statistics(statistics& stats, observable_area area);
		std::string serialize_statistics();
		void thread_init(thread_data& gen, std::uint64_t seed, std::size_t thread_id);
		void dump();
	};
}
