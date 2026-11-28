#include "../ddos.hpp"

#include <ranges>
#include <controllers/monitor.hpp>

tests::load::ddos_plugin* tests::load::ddos_plugin::instance_ = nullptr;

tests::load::ddos_plugin& tests::load::ddos_plugin::instance()
{
	return *instance_;
}

void tests::load::ddos_plugin::initAndStart(const Json::Value& cfg)
{
	try
	{
		instance_ = this;
		safe_init(cfg);
		start();
	}
	catch (const std::exception& e)
	{
		LOG_FATAL << "failed to startup from config: " << e.what();
		std::exit(2);
	}
}

void tests::load::ddos_plugin::start()
{
	std::unique_lock lock{rw_mutex};
	if (status.state != dynamic_status::state_t::stopped) return;
	status.state = dynamic_status::state_t::initializing;

	const std::size_t nthreads = drogon::app().getThreadNum();

	// Seeds
	std::vector< std::uint32_t > seeds;
	std::size_t start_seed = std::random_device{}();
	auto iota = std::views::iota(start_seed, start_seed + nthreads + 1);
	std::seed_seq seq{iota.begin(), iota.end()};
	seeds.resize(nthreads + 1);
	seq.generate(seeds.begin(), seeds.end());
	threads->engine.seed(seeds[nthreads]);

	// Startup
	double user_qps = 2 / status.certain_time.count();
	LOG_INFO << std::format("Loader started: with QPS = {} ({} per thread, {} per user)",
				user_qps * status.users_per_thread * nthreads,
				user_qps * status.users_per_thread, user_qps);
	status.graph_start = clock::now();
	status.state = dynamic_status::state_t::initializing;
	status.commit_task = drogon::app().getLoop()->runEvery(static_cast< double >(status.frame_delay.count()),
				[this]{print_status();});

	lock.unlock();

	threads.init([this, nthreads, &seeds]
				(thread_data& data, std::size_t thread_id)
				{
					if (thread_id == nthreads) return;
					thread_init(data, seeds[thread_id], thread_id);
				});

	std::shared_lock read_lock{rw_mutex};
	if (status.state == dynamic_status::state_t::stopped) return;
	if (status.logged_users == nthreads * status.users_per_thread)
	{
		read_lock.unlock();
		escalate();
	}
}
void tests::load::ddos_plugin::thread_init(thread_data& gen, std::uint64_t seed, std::size_t thread_id)
{
	gen.engine.seed(seed);
	for (int i = 0; i < static_cast< int >(observable_area::last); i++)
	{
		observable_area area = static_cast< observable_area >(i);
		statistics::resp_t& resp = gen.stats[area];
		resp.last_request = resp.last_response = resp.start = status.graph_start;
		dynamic_status::dynamic_area_frame& frame = status[area];
		resp.qos_groups.resize(0);
		resp.qos_groups.resize(frame.qos_groups.size() + 1, 0);
	}
	gen.users.resize(status.users_per_thread, {.caret_time = status.graph_start});
	trantor::EventLoop* loop = drogon::app().getIOLoop(thread_id);
	std::shared_lock lock{rw_mutex};
	for (std::size_t i = 0; i < status.users_per_thread; i++)
	{
		drogon::HttpClientPtr& client = gen.users[i].client;
		if (!client) client = drogon::HttpClient::newHttpClient(gateway_, loop, false, false);
		client->enableCookies();
		if (gen.users[i].local_id == 0) gen.users[i].local_id = ++status.local_users;
		if (i < status.max_init_thread_queue)
			gen.users[i].switch_state(virtual_user::state_t::auth_request, lock);
	}
}

void tests::load::ddos_plugin::escalate()
{
	std::unique_lock lock{rw_mutex};
	if (status.state != dynamic_status::state_t::initializing) return;

	status.state = dynamic_status::state_t::running;
	lock.unlock();

	clock::time_point now = clock::now();
	threads.init([this, now](thread_data& data, std::size_t thread_id)
	{
		data.stats.calc.resp.start = now;
		for (virtual_user& user: data.users)
		{
			user.caret_time = now;
			user.run();
		}
	});
}
void tests::load::ddos_plugin::stop()
{
	std::unique_lock lock{rw_mutex};
	status.state = dynamic_status::state_t::stopped;
	std::string stats = serialize_statistics();
	LOG_INFO << stats;
	socket::instance().broadcast(stats);
}
void tests::load::ddos_plugin::shutdown()
{
	stop();
	dump();
	LOG_INFO << "Loader quit";
}
void tests::load::ddos_plugin::dump()
{
	std::unique_lock lock{rw_mutex};
	drogon::app().getIOLoop(drogon::app().getThreadNum())->invalidateTimer(status.commit_task);
	logins_file_.close();
	std::filesystem::path patch_filename = std::filesystem::temp_directory_path()
			/ "planner-load-test-logins.txt";
	std::error_code err;
	try
	{
		std::ofstream patch_file{patch_filename};
		threads.init([this, &patch_file](thread_data& data, std::size_t thread_id)
		{
			for (const virtual_user& user: data.users)
			{
				if (user.global_id == -1) continue;
				patch_file << user.global_id << " " << user.login.nickname << " " << user.login.password;
				if (!user.login.refresh_token.empty())
				{
					patch_file << " " << user.login.refresh_token;
				}
				patch_file << '\n';
			}
		});
		patch_file.close();
		std::filesystem::create_directories(logins_filename_.parent_path(), err);
		if (err)
		{
			LOG_ERROR << "failed to create dirictories for logins file ("
					<< logins_filename_ << "): " << err.message();
		}
		else
		{
			std::filesystem::copy_file(patch_filename, logins_filename_,
						std::filesystem::copy_options::overwrite_existing, err);
			if (err)
				LOG_ERROR << "failed to replace logins file with new one ("
						<< patch_filename << "): " << err.message();
			else
				LOG_INFO << "Successfully updated logins file";
		}
	}
	catch (...)
	{}
	std::filesystem::remove(patch_filename, err);
	if (err)
		LOG_ERROR << "failed to remove temp logins file ("
				<< patch_filename << "): " << err.message();
}
