#include "../ddos.hpp"

using std::chrono::duration_cast;
using std::chrono::milliseconds;

void tests::load::dynamic_status::set_qps(double user_qps)
{
	certain_time = float_seconds{2 / user_qps};
	wait_distr = decltype(wait_distr){0, certain_time.count()};
	if ((certain_time <= float_seconds{1e-7})
				|| std::isnan(certain_time.count()) || std::isinf(certain_time.count()))
		throw std::invalid_argument("invalid query density value");
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
	recieved += rhs.recieved;

	net_error += rhs.net_error;
	timeout += rhs.timeout;
	k2xx += rhs.k2xx;
	k400 += rhs.k400;
	k401 += rhs.k401;
	k4xx += rhs.k4xx;
	k5xx += rhs.k5xx;

	qos_groups.resize(std::max(qos_groups.size(), rhs.qos_groups.size()));
	for (std::size_t i = 0; i < rhs.qos_groups.size(); i++)
	{
		qos_groups[i] += rhs.qos_groups[i];
	}
	return *this;
}
tests::load::statistics::auth_t& tests::load::statistics::auth_t::operator+=(const auth_t& rhs) noexcept
{
	logged += rhs.logged;
	refreshed += rhs.refreshed;
	registered += rhs.registered;
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
tests::load::statistics::resp_t& tests::load::statistics::operator[](observable_area area) noexcept
{
	switch (area)
	{
	case observable_area::auth:
		return auth.resp;
	case observable_area::sched:
		return calc.resp;
	default:
		std::unreachable();
	}
}
tests::load::dynamic_status::dynamic_area_frame& tests::load::dynamic_status::operator[](observable_area area)
{
	return areas[static_cast< int >(area)];
}
void tests::load::statistics::resp_t::push(cell id, clock::time_point start,
			drogon::ReqResult res, const drogon::HttpResponsePtr& resp)
{
	ddos_plugin& plugin = ddos_plugin::instance();
	dynamic_status::dynamic_area_frame& frame = plugin.status[area];

	pending--;
	last_response = clock::now();
	recieved++;
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
			k2xx_durs += (last_response - start);
			float_seconds fdelta{last_response - start};
			std::ptrdiff_t qos = std::lower_bound(frame.qos_groups.begin(), frame.qos_groups.end(), fdelta)
					- frame.qos_groups.begin();
			qos += ((qos != 0) && (qos != frame.qos_groups.size()));
			qos_groups[static_cast< std::size_t >(qos)]++;
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
}
