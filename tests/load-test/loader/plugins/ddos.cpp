#include "ddos.hpp"

#include <print>

#undef max
#undef min

tests::load::ddos_plugin* tests::load::ddos_plugin::instance_ = nullptr;

tests::load::ddos_plugin& tests::load::ddos_plugin::instance()
{
	return *instance_;
}
tests::load::statistics::resp_t& tests::load::statistics::resp_t::operator+=(const resp_t& rhs) noexcept
{
	start = std::min(start, rhs.start);
	last_request = std::max(last_request, rhs.last_request);
	last_response = std::max(last_response, rhs.last_response);
	durs += rhs.durs;
	k2xx_durs += rhs.k2xx_durs;

	pending += rhs.pending;
	good += rhs.good;
	bad += rhs.bad;

	net_error += rhs.net_error;
	timeout += rhs.timeout;
	k2xx += rhs.k2xx;
	k400 += rhs.k400;
	k401 += rhs.k401;
	k4xx += rhs.k4xx;
	k5xx += rhs.k5xx;
	return *this;
}
tests::load::statistics::auth_t& tests::load::statistics::auth_t::operator+=(const auth_t& rhs) noexcept
{
	logged += rhs.logged;
	expired_tokens += rhs.expired_tokens;
	unique_registrate_failed += rhs.unique_registrate_failed;
	resp += rhs.resp;
	return *this;
}
tests::load::statistics::calc_t& tests::load::statistics::calc_t::operator+=(const calc_t& rhs) noexcept
{
	resp += rhs.resp;
	return *this;
}
tests::load::statistics& tests::load::statistics::operator+=(const statistics& rhs) noexcept
{
	auth += rhs.auth;
	calc += rhs.calc;
	return *this;
}

void tests::load::ddos_plugin::shutdown()
{
	if (sended < max_queries)
	{
		std::println("Ddos hasn't sent {} queries", max_queries - sended.load());
	}
	clock::duration dur = clock::now() - start_;
	std::size_t err = errors.load(), succ = success.load(), snd = err + succ;
	std::println("error rate {} ({}/{}) duration {}s",
				static_cast< float >(err) / snd, err, snd, float_seconds{dur}.count());

	statistics stats;
	threads.init([&stats](thread_data& data, std::size_t thread_id){stats += data.stats;});

	statistics::cell sended = stats.calc.resp.bad + stats.calc.resp.good;
	float_seconds req_sec = stats.calc.resp.last_request - start_;
	double qps = (sended + stats.calc.resp.pending) / req_sec.count();
	float_seconds resp_sec = stats.calc.resp.last_request - start_;
	double rps = sended / resp_sec.count();

	double err_rate = static_cast< double >(stats.calc.resp.bad) / (sended);
	statistics::cell err_denum = std::max(stats.calc.resp.bad, statistics::cell{1});
	double timeout_rate = static_cast< double >(stats.calc.resp.timeout) / err_denum;
	double net_err_rate = static_cast< double >(stats.calc.resp.net_error) / err_denum;
	double k400_rate = static_cast< double >(stats.calc.resp.k400) / err_denum;
	double k401_rate = static_cast< double >(stats.calc.resp.k401) / err_denum;
	double k4xx_rate = static_cast< double >(stats.calc.resp.k4xx) / err_denum;
	double k5xx_rate = static_cast< double >(stats.calc.resp.k5xx) / err_denum;
	LOG_INFO << std::format(
R"fmt(Calc queries statistics:
	Pending {}
	QPS {:.4f} (duration {:.3f}s)
	RPS {:.4f} (duration {:.3f}s)
	Avg latency {:.3f}s ({} resp)
	Avg latency for k2xx only {:.3f}s ({} resp)
	Errors ({}/{}):
		rate {:.2f}%
		timeout {:.2f}% ({})
		net error {:.2f}% ({})
		http k400 {:.2f}% ({})
		http k401 {:.2f}% ({})
		http other k4xx {:.2f}% ({})
		http k5xx {:.2f}% ({})
)fmt",
				stats.calc.resp.pending, qps, req_sec.count(), rps, resp_sec.count(),
				stats.calc.resp.durs.count() / sended, sended,
				stats.calc.resp.k2xx_durs.count() / std::max(stats.calc.resp.good, statistics::cell{1}),
				stats.calc.resp.good,
				stats.calc.resp.bad, sended,
				err_rate * 100,
				timeout_rate * 100, stats.calc.resp.timeout,
				net_err_rate * 100, stats.calc.resp.net_error,
				k400_rate * 100, stats.calc.resp.k400,
				k401_rate * 100, stats.calc.resp.k401,
				k4xx_rate * 100, stats.calc.resp.k4xx,
				k5xx_rate * 100, stats.calc.resp.k5xx
			);

}
void tests::load::ddos_plugin::print_status()
{
	std::size_t err = errors.load(), succ = success.load(), snd = sended.load();
	clock::time_point now = clock::now();
	clock::duration dur = now - last_;
	double qps = (snd - last_sended_) / float_seconds{dur}.count();
	last_ = now;
	last_sended_ = snd;
	std::println("[{:%F %T}] error rate {} ({}/{}), {} in progress, QPS {}",
				std::chrono::system_clock::now(), static_cast< float >(err) / (err + succ),
				err, err + succ, snd - err - succ, qps);
}
void tests::load::virtual_user::send_request(std::uint_least64_t id)
{
	using std::chrono::duration_cast;
	using drogon::ReqResult;
	using resp_t = drogon::HttpResponsePtr;

	ddos_plugin& plugin = ddos_plugin::instance();
	while (true)
	{
		clock::time_point now = clock::now();
		statistics::resp_t& resp = plugin.threads->stats.calc.resp;
		resp.last_request = now;
		if (id != 0)
		{
			drogon::HttpRequestPtr req = drogon::HttpRequest::newHttpRequest();
			req->setMethod(drogon::HttpMethod::Get);
			req->setPath("/api/scheduler/status");
			client->sendRequest(req,
						[this, id, now](ReqResult res, const resp_t& resp)
						{req_callback(id, now, res, resp);}, plugin.timeout.count());
			resp.pending++;
			LOG_DEBUG << "@" << id << " local user " << local_id << " sended";
		}
		id = ++plugin.sended;
		if (id <= plugin.max_queries)
		{
			float_seconds delay{plugin.wait_distr(plugin.threads->engine)};
			caret_time += duration_cast< clock::duration >(delay);
			if (caret_time > now)
			{
				drogon::app().getLoop()->runAfter(caret_time - now,
							[this, id]{send_request(id);});
				return;
			}
			continue;
		}
		return;
	}
}
void tests::load::virtual_user::req_callback(std::uint_least64_t id, clock::time_point start,
			drogon::ReqResult res, drogon::HttpResponsePtr resp)
{
	ddos_plugin& plugin = ddos_plugin::instance();
	plugin.threads->stats.calc.resp.push(id, start, res, resp);
	if (plugin.errors + plugin.success >= plugin.max_queries)
	{
		std::println("quit");
		drogon::app().quit();
	}
}
void tests::load::statistics::resp_t::push(cell id, clock::time_point start,
			drogon::ReqResult res, const drogon::HttpResponsePtr& resp)
{
	using std::chrono::duration_cast;
	using std::chrono::milliseconds;

	ddos_plugin& plugin = ddos_plugin::instance();
	pending--;
	last_response = clock::now();
	durs += (last_response - start);
	if (res == drogon::ReqResult::Ok)
	{
		milliseconds delta = duration_cast< milliseconds >(last_response - start);
		LOG_DEBUG << "#" << id << " " << delta.count() << "ms " << " k" << resp->getStatusCode();
		drogon::HttpStatusCode code = resp->statusCode();
		if (code >= 500) k5xx++;
		else if (code == 401) k401++;
		else if (code == 400) k400++;
		else if (code >= 400) k4xx++;
		else if ((code >= 200) && (code < 300))
		{
			plugin.success.fetch_add(1);
			good++;
			k2xx_durs += (last_response - start);
			return;
		}
	}
	else if (res == drogon::ReqResult::Timeout)
	{
		timeout++;
		LOG_DEBUG << "!" << id << " bad: timeout";
	}
	else
	{
		net_error++;
		LOG_DEBUG << "!" << id << "bad: net error";
	}
	plugin.errors.fetch_add(1);
	bad++;
}
