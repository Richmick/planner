#include "../ddos.hpp"

#include <print>

using std::chrono::duration_cast;
using drogon::ReqResult;
using resp_t = drogon::HttpResponsePtr;

void tests::load::virtual_user::run(std::uint_least64_t id)
{
	switch (state)
	{
	case state_t::auth_request:
	case state_t::logging:
	case state_t::refreshing:
	case state_t::registrating:
		authentificate(id);
		break;
	[[likely]]
	case state_t::running:
		schedule(id);
		break;
	}
}
void tests::load::virtual_user::switch_state(state_t new_state, std::shared_lock< std::shared_mutex >& lock)
{
	if (new_state == state_t::auth_request)
	{
		if (state != state_t::auth_request) return;
		if (!login.refresh_token.empty()) new_state = state_t::refreshing;
		else if (!login.nickname.empty()) new_state = state_t::logging;
		else new_state = state_t::registrating;
		if (state != state_t::auth_request)
		{
			state = new_state;
			return;
		}
	}

	state_t prev_state = std::exchange(state, new_state);
	ddos_plugin& plugin = ddos_plugin::instance();
	switch (prev_state)
	{
	case state_t::auth_request:
	case state_t::registrating:
	case state_t::refreshing:
	case state_t::logging:
	{
		if ((new_state == state_t::refreshing) || (new_state == state_t::logging)
					|| (new_state == state_t::registrating))
			return authentificate(++plugin.id_generator);
		thread_data& thread = *plugin.threads;
		if (thread.stats.auth.inside < plugin.status.users_per_thread)
		{
			thread.users[thread.stats.auth.inside].switch_state(state_t::auth_request, lock);
		}
		break;
	}
	default:
		assert(prev_state != state_t::auth_request);
		if (new_state == state_t::auth_request)
			return authentificate(++plugin.id_generator);
		break;
	}
	if (plugin.status.state == dynamic_status::state_t::initializing)
	{
		if (plugin.status.logged_users == drogon::app().getThreadNum() * plugin.status.users_per_thread)
		{
			if (lock) lock.unlock();
			plugin.escalate();
		}
		return;
	}
	run();
}

void tests::load::dynamic_status::send_routine(statistics::cell id, virtual_user& user, observable_area area)
{
	ddos_plugin& plugin = ddos_plugin::instance();
	thread_data& thread = *plugin.threads;
	statistics::resp_t& stats = thread.stats[area];
	stats.last_request = clock::now();
	stats.pending++;
	plugin.status[area].sended++;
	LOG_DEBUG << "--" << name_of(area) << "-- send @" << id << " user$" << user.local_id;

}
void tests::load::dynamic_status::callback_routine(statistics::cell id, virtual_user& user, observable_area area,
			clock::time_point start, drogon::ReqResult res, const drogon::HttpResponsePtr& resp)
{
	ddos_plugin& plugin = ddos_plugin::instance();
	dynamic_status::dynamic_area_frame& frame = plugin.status[area];
	thread_data& thread = *plugin.threads;
	statistics::resp_t& stats = thread.stats[area];
	plugin.status[area].recieved++;
	frame.recieved++;
	stats.push(id, start, res, resp);
}

void tests::load::virtual_user::schedule(std::uint_least64_t id)
{
	ddos_plugin& plugin = ddos_plugin::instance();
	dynamic_status::dynamic_area_frame& frame = plugin.status[observable_area::sched];
	while (true)
	{
		std::shared_lock lock{plugin.rw_mutex};
		if (plugin.status.state == dynamic_status::state_t::stopped) return;

		clock::time_point now = clock::now();
		if (id != 0)
		{
			drogon::HttpRequestPtr req = drogon::HttpRequest::newHttpRequest();
			req->setMethod(drogon::HttpMethod::Get);
			req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
			req->setPath("/api/scheduler/status");
			client->sendRequest(req,
						[this, id, now](ReqResult res, const resp_t& resp)
						{sched_cb(id, now, res, resp);}, frame.timeout.count());
			plugin.status.send_routine(id, *this, observable_area::sched);
		}
		id = ++plugin.id_generator;
		if (++frame.sended <= frame.max_queries)
		{
			float_seconds delay{plugin.status.wait_distr(plugin.threads->engine)};
			caret_time += duration_cast< clock::duration >(delay);
			if (caret_time > now)
			{
				drogon::app().getLoop()->runAfter(caret_time - now,
							[this, id]{schedule(id);});
				return;
			}
			continue;
		}
		return;
	}
}
void tests::load::virtual_user::authentificate(std::uint_least64_t id)
{
	ddos_plugin& plugin = ddos_plugin::instance();
	dynamic_status::dynamic_area_frame& frame = plugin.status[observable_area::auth];

	std::shared_lock lock{plugin.rw_mutex};
	if (plugin.status.state == dynamic_status::state_t::stopped) return;
	if (++frame.sended > frame.max_queries) return;

	thread_data& thread = *plugin.threads;
	drogon::HttpRequestPtr req = drogon::HttpRequest::newHttpRequest();
	req->setMethod(drogon::HttpMethod::Post);
	req->setContentTypeCode(drogon::CT_APPLICATION_JSON);

	switch_state(state_t::auth_request, lock); // update state if needed
	switch (state)
	{
	case state_t::logging:
		req->setPath("/api/login");
		req->setBody(std::format("{{\"nickname\":\"{}\",\"password\":\"{}\"}}",
						login.nickname, login.password));
		LOG_DEBUG << "user$" << local_id << " reused name: \"" << login.nickname << '"';
		break;
	case state_t::refreshing:
		req->setPath("/api/login");
		req->setBody(std::format("{{\"refresh\":\"{}\"}}", login.refresh_token));
		LOG_DEBUG << "user$" << local_id << " reused token: \"" << login.refresh_token << '"';
		break;
	case state_t::registrating:
		req->setPath("/api/auth/logup");
		login.nickname = "user";
		login.nickname += std::to_string(thread.engine());
		login.password.clear();
		login.refresh_token.clear();
		while (login.password.size() < 8)
			login.password += std::to_string(thread.engine());

		req->setBody(std::format("{{\"nickname\":\"{}\",\"email\":\"{}\",\"password\":\"{}\"}}",
						login.nickname, login.nickname + "@nomail.com", login.password));
		LOG_DEBUG << "user$" << local_id << " chose login: \"" << login.nickname << '"';
		break;

	default:
		std::unreachable();
	}
	plugin.status.send_routine(id, *this, observable_area::auth);
	client->sendRequest(req,
				[this, id, now = clock::now()](ReqResult res, const resp_t& resp)
				{auth_cb(id, now, res, resp);}, frame.timeout.count());
}
void tests::load::virtual_user::sched_cb(std::uint_least64_t id, clock::time_point start,
			drogon::ReqResult res, drogon::HttpResponsePtr resp)
{
	ddos_plugin& plugin = ddos_plugin::instance();
	std::shared_lock lock{plugin.rw_mutex};
	if (plugin.status.state == dynamic_status::state_t::stopped) return;

	statistics::calc_t& calc = plugin.threads->stats.calc;
	plugin.status.callback_routine(id, *this, observable_area::sched, start, res, resp);
	if ((res == drogon::ReqResult::Ok) && (resp->statusCode() == 200))
	{
		plugin.status[observable_area::sched].success++;
		calc.resp.good++;
	}
}
void tests::load::virtual_user::auth_cb(std::uint_least64_t id, clock::time_point start,
			drogon::ReqResult res, drogon::HttpResponsePtr resp)
{
	ddos_plugin& plugin = ddos_plugin::instance();
	std::shared_lock lock{plugin.rw_mutex};
	if (plugin.status.state == dynamic_status::state_t::stopped) return;

	statistics::auth_t& auth = plugin.threads->stats.auth;

	plugin.status.callback_routine(id, *this, observable_area::auth, start, res, resp);
	float_seconds after_sec{0};
	if (res == drogon::ReqResult::Ok)
	{
		std::shared_ptr< Json::Value > json;
		switch (resp->statusCode())
		{
		case drogon::k200OK:
			json = resp->getJsonObject();
			try
			{
				if (json && json->isObject())
				{
					if (json->is< std::uint64_t >())
					{
						global_id = json->as< std::uint64_t >();
					}
					else
					{
						global_id = json->get("id", Json::Value::nullSingleton()).as< std::uint64_t >();
						if (json->isMember("refresh"))
							login.refresh_token = json->get("refresh", Json::Value::nullSingleton()).asString();
					}

					plugin.status.logged_users++;
					auth.inside++;
					plugin.status[observable_area::auth].success++;
					auth.resp.good++;
					auth.logged++;
					return switch_state(state_t::running, lock);
				}
				LOG_ERROR << "got invalid json: " << resp->getJsonError();
			}
			catch (...)
			{
				LOG_ERROR << "invalid auth response with code 200: " << resp->body();
			}
			break;
		case drogon::k204NoContent:
			switch (state)
			{
			case state_t::logging:
				LOG_ERROR << "auth response with code 204 for login request (json expected)";
				break;
			case state_t::running:
				LOG_ERROR << "sched response with code 204 (no-body)";
				break;
			[[likely]] case state_t::registrating:
				global_id = -2;
				plugin.status[observable_area::auth].success++;
				auth.resp.good++;
				auth.registered++;
				return switch_state(state_t::logging, lock);
			case state_t::refreshing:
				plugin.status.logged_users++;
				auth.inside++;
				plugin.status[observable_area::auth].success++;
				auth.resp.good++;
				auth.refreshed++;
				return switch_state(state_t::running, lock);
			default:
				break;
			}
			break;
		case drogon::k400BadRequest:
			if (state == state_t::registrating)
			{
				login.nickname.clear();
				login.password.clear();
				login.refresh_token.clear();
				auth.unique_registrate_failed++;
				return switch_state(state, lock);
			}
			if (state == state_t::refreshing)
			{
				login.refresh_token.clear();
				auth.expired_tokens++;
				return switch_state(state_t::logging, lock);
			}
			break;
		case drogon::k429TooManyRequests:
			{
				after_sec = plugin.status.retry_429_timeout;
				const std::string& after = resp->getHeader("Retry-After");
				if (after.empty()) break;
				try
				{
					after_sec = float_seconds{std::stol(after)};
				}
				catch (...)
				{
					LOG_ERROR << "server sent k429 (too many requests) with unrecognizable retry-after: " << after;
				}
			}
			break;
		}
	}
	LOG_ERROR << "--auth-- failed user$" << local_id << " resp: " << resp->body();
	retry++;
	statistics::cell next_id = ++plugin.id_generator;
	if (after_sec.count() != 0)
		authentificate(next_id);
	else
		drogon::app().getLoop()->runAfter(after_sec.count(), [this, next_id]{authentificate(next_id);});
}
