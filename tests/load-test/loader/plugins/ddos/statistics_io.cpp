#include "../ddos.hpp"

#include <print>
#include <controllers/monitor.hpp>

#undef max
#undef min

std::string tests::load::ddos_plugin::serialize_area_statistics(statistics& stats, observable_area area)
{
	statistics::resp_t& resp = stats[area];

	float_seconds req_sec = resp.last_request - resp.start;
	double qps = (resp.recieved + resp.pending) / req_sec.count();
	float_seconds resp_sec = resp.last_response - resp.start;
	double rps = resp.recieved / resp_sec.count();
	
	statistics::cell err = resp.recieved - resp.good;
	double err_rate = static_cast< double >(err) / (resp.recieved);
	statistics::cell err_denum = std::max(err, statistics::cell{1});
	double timeout_rate = static_cast< double >(resp.timeout) / err_denum;
	double net_err_rate = static_cast< double >(resp.net_error) / err_denum;
	double k400_rate = static_cast< double >(resp.k400) / err_denum;
	double k401_rate = static_cast< double >(resp.k401) / err_denum;
	double k4xx_rate = static_cast< double >(resp.k4xx) / err_denum;
	double k5xx_rate = static_cast< double >(resp.k5xx) / err_denum;
	std::string result = std::format(
R"fmt(Queries statistics for {}:
	QPS {:.4f} (duration {:.3f}s)
	RPS {:.4f} (duration {:.3f}s)
	Avg latency {:.3f}s ({} resp)
	Avg latency for k2xx only {:.3f}s ({} resp)
	Total {}
	Pending {}
	Success {}
	Errors {}:
		rate {:.2f}%
		timeout {:.2f}% ({})
		net error {:.2f}% ({})
		http k400 {:.2f}% ({})
		http k401 {:.2f}% ({})
		http other k4xx {:.2f}% ({})
		http k5xx {:.2f}% ({})
	QOS groups:
)fmt",
				name_of(area), qps, req_sec.count(), rps, resp_sec.count(),
				resp.durs.count() / resp.recieved, resp.recieved,
				resp.k2xx_durs.count() / std::max(resp.good, statistics::cell{1}), resp.good,
				resp.recieved, resp.pending, resp.good, err,
				err_rate * 100,
				timeout_rate * 100, resp.timeout,
				net_err_rate * 100, resp.net_error,
				k400_rate * 100, resp.k400,
				k401_rate * 100, resp.k401,
				k4xx_rate * 100, resp.k4xx,
				k5xx_rate * 100, resp.k5xx
			);
	for (std::size_t i = 0; i < resp.qos_groups.size(); i++)
	{
		result += std::format("\t\t{}) [{:.3f};{:.3f}) - {}\n", i, (i == 0 ? 0: status[area].qos_groups[i - 1].count()),
					(i + 1 >= resp.qos_groups.size()
						? std::numeric_limits< double >::infinity()
						: status[area].qos_groups[i].count()), resp.qos_groups[i]);
	}
	return result;
}
std::string tests::load::ddos_plugin::serialize_statistics()
{
	statistics stats;
	threads.init([&stats](thread_data& data, std::size_t thread_id) {stats += data.stats;});

	std::string result;
	result += serialize_area_statistics(stats, observable_area::auth);
	result += std::format(
R"fmt(Auth distribution:
	Login {}
	Refresh {} (expired {})
	Logup {} (unique violations {})
)fmt",
				stats.auth.logged,
				stats.auth.refreshed, stats.auth.expired_tokens,
				stats.auth.registered, stats.auth.unique_registrate_failed
			);
	result += serialize_area_statistics(stats, observable_area::sched);
	return result;
}

void tests::load::ddos_plugin::print_status()
{
	std::shared_lock lock{rw_mutex};

	std::string msg = std::format("status:");
	LOG_INFO << msg;
	socket::instance().broadcast(msg);

	/*statistics::cell err = status.recieved.load(), succ = status.success.load(), snd = status.sended.load();
	clock::time_point now = clock::now();
	clock::duration dur = now - status.last_frame.now;
	double qps = (snd - last_sended_) / float_seconds{dur}.count();
	last_ = now;
	last_sended_ = snd;
	std::println("[{:%F %T}] error rate {} ({}/{}), {} in progress, QPS {}",
				std::chrono::system_clock::now(), static_cast< float >(err) / (err + succ),
				err, err + succ, snd - err - succ, qps);*/
}
