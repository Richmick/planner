#include "../ddos.hpp"

void tests::load::ddos_plugin::safe_init(const Json::Value& cfg)
{
	std::unique_lock lock{rw_mutex};

	// Global
	gateway_ = cfg.get("address", "https://127.0.0.1").asString();
	status.users_per_thread = cfg.get("users_per_thread", status.users_per_thread).as< std::size_t >();
	status.frame_delay = float_seconds{cfg.get("frame_delay", status.frame_delay.count()).as< double >()};
	status.max_init_thread_queue = cfg.get("start_max_thread_auth_queue", status.max_init_thread_queue).as< std::size_t >();
	Json::String scheme = cfg.get("scheme", "sequence").asString();
	const std::size_t nthreads = drogon::app().getThreadNum();

	// Logs
	Json::Value log_files = cfg.get("log_files", Json::Value::nullSingleton());
	if (!log_files.isNull())
	{
		gateway_logs = log_files.get("gateway", "gateway.log").asString();
		gateway_access_logs = log_files.get("gateway_access", "gateway-access.log").asString();
		users_logs = log_files.get("users", "users.log").asString();
		users_access_logs = log_files.get("users_access", "users-access.log").asString();
	}

	if (scheme == "sequence")
	{}
	else throw std::invalid_argument("invalid work scheme");

	// Pools
	Json::Value pools = cfg.get("pools", Json::Value::nullSingleton());
	for (const std::string& area_name: pools.getMemberNames())
	{
		observable_area area = from_name(area_name);
		if (area == observable_area::last)
			throw std::invalid_argument("invalid pool (area)");
		dynamic_status::dynamic_area_frame& frame = status[area];
		const Json::Value& pool = pools[area_name];

		frame.max_queries = pool.get("max_queries", frame.max_queries).as< statistics::cell >();
		frame.timeout = float_seconds{pool.get("timeout", frame.timeout.count()).as< double >()};
		frame.retries = pool.get("retries", frame.retries).as< unsigned >();
		const Json::Value& qos = pool["QOS_groups"];
		if (!qos.isNull())
		{
			if (!qos.isArray()) throw std::invalid_argument("QOS_groups must be array");
			for (const Json::Value& i: qos)
			{
				frame.qos_groups.push_back(float_seconds{i.as< double >()});
			}
		}
	}
	Json::Value qps_json = cfg.get("query_density", nthreads * status.users_per_thread);
	double user_qps = 1.0;
	if (qps_json.isNumeric()) user_qps = qps_json.as< double >() / nthreads / status.users_per_thread;
	else
	{
		Json::String type = qps_json.get("type", "global").asString();
		double value = qps_json.get("value", 1.0).as< double >();
		if (type == "global") user_qps = value / nthreads / status.users_per_thread;
		else if (type == "thread") user_qps = value / status.users_per_thread;
		else if (type == "user") user_qps = value;
		else throw std::invalid_argument("invalid query density type");
	}
	status.set_qps(user_qps);

	// Login pools
	logins_filename_ = cfg.get("logins", "logins.txt").asString();
	logins_file_.open(logins_filename_);
	std::size_t caret_id = 0, filled_users_per_thread = 0;
	if (logins_file_.is_open())
	{
		std::string line;
		while (std::getline(logins_file_, line))
		{
			std::istringstream in{line};
			virtual_user user{.caret_time = status.graph_start};
			if (!(in >> user.global_id >> user.login.nickname >> user.login.password)) continue;
			if (!(in >> user.login.refresh_token)) user.login.refresh_token.clear();

			threads.init([caret_id, &user, i = filled_users_per_thread, ups = status.users_per_thread]
					(thread_data& data, std::size_t thread_id)
			{
				if (thread_id != caret_id) return;
				data.users.resize(ups, {.caret_time = user.caret_time});
				data.users[i] = std::move(user);
			});
			if (++caret_id == nthreads)
			{
				filled_users_per_thread++;
				caret_id = 0;
				if (filled_users_per_thread == status.users_per_thread) break;
			}
		}
	}
}
