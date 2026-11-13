#include "ddos.hpp"

void tests::load::ddos_plugin::initAndStart(const Json::Value& cfg)
{
	try
	{
		instance_ = this;
		safe_init(cfg);
	}
	catch (const std::exception& e)
	{
		LOG_FATAL << "failed to startup from config: " << e.what();
		std::exit(2);
	}
}
void tests::load::ddos_plugin::safe_init(const Json::Value& cfg)
{
	// Global
	gateway_ = cfg.get("address", "https://127.0.0.1").asString();
	max_queries = cfg.get("max_queries", 10).as< std::uint64_t >();
	users_per_thread = cfg.get("users_per_thread", 1).as< std::size_t >();
	timeout = std::chrono::seconds{cfg.get("timeout", 10).as< unsigned >()};
	float_seconds stdout_log_delay{cfg.get("stdout_log_delay", 5).as< unsigned >()};
	const std::size_t nthreads = drogon::app().getThreadNum();

	// QPS
	Json::Value qps_json = cfg.get("query_density", nthreads * users_per_thread);
	double user_qps = 1.0;
	if (qps_json.isNumeric()) user_qps = qps_json.as< double >() / nthreads / users_per_thread;
	else
	{
		Json::String type = qps_json.get("type", "global").asString();
		double value = qps_json.get("value", 1.0).as< double >();
		if (type == "global") user_qps = value / nthreads / users_per_thread;
		else if (type == "thread") user_qps = value / users_per_thread;
		else if (type == "user") user_qps = value;
		else throw std::invalid_argument("invalid query density type");
	}
	certain_time = float_seconds{2 / user_qps};
	wait_distr = decltype(wait_distr){0, certain_time.count()};
	if ((certain_time <= float_seconds{1e-7})
				|| std::isnan(certain_time.count()) || std::isinf(certain_time.count()))
		throw std::invalid_argument("invalid query density value");

	// Startup
	LOG_INFO << std::format("Loader started: requested {} queries with QPS = {} ({} per thread, {} per user)",
				max_queries, user_qps * users_per_thread * nthreads,
				user_qps * users_per_thread, user_qps);
	last_ = start_ = clock::now();
	drogon::app().getLoop()->runEvery(static_cast< double >(stdout_log_delay.count()),
				[this]{print_status();});

	std::random_device rnd_device;
	threads.init([this, &rnd_device]
				(thread_data& data, std::size_t thread_id)
				{thread_init(data, rnd_device(), thread_id);});
}
void tests::load::ddos_plugin::thread_init(thread_data& gen, std::uint64_t seed, std::size_t thread_id)
{
	gen.engine.seed(seed);
	gen.users.resize(users_per_thread, {.caret_time = clock::now()});
	trantor::EventLoop* loop = drogon::app().getIOLoop(thread_id);
	for (std::size_t i = 0; i < users_per_thread; i++)
	{
		drogon::HttpClientPtr& client = gen.users[i].client;
		client = drogon::HttpClient::newHttpClient(gateway_, loop, false, false);
		client->enableCookies();
		gen.users[i].local_id = thread_id * users_per_thread + i;
		gen.users[i].send_request();
	}
}
