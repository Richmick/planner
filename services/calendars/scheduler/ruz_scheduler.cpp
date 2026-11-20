#include "ruz_scheduler.hpp"

#include "ruz_client.hpp"
#include "ruz_storage.hpp"

#include <drogon/plugins/Plugin.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>

namespace api::calendars::ruz
{
	using namespace std::chrono;

	namespace
	{
		double config_number(const Json::Value& config, const char* key, double fallback, double maximum)
		{
			const auto value = config.get(key, fallback);
			if (!value.isNumeric() || !std::isfinite(value.asDouble()) || value.asDouble() <= 0
					|| value.asDouble() > maximum)
				throw std::invalid_argument(std::string("invalid RUZ configuration: ") + key);
			return value.asDouble();
		}

		int config_integer(const Json::Value& config, const char* key, int fallback, int maximum)
		{
			const auto value = config.get(key, fallback);
			if (!value.isInt() || value.asInt() <= 0 || value.asInt() > maximum)
				throw std::invalid_argument(std::string("invalid RUZ configuration: ") + key);
			return value.asInt();
		}

		trantor::Date timer_date(utc_seconds time)
		{
			return trantor::Date(duration_cast< microseconds >(utc_clock::to_sys(time).time_since_epoch()).count());
		}

		struct sync_job
		{
			calendar target;
			utc_seconds from;
			utc_seconds to;
			friend auto operator<=>(const sync_job&, const sync_job&) = default;
		};

		struct lesson_refresh
		{
			calendar target;
			local_days date;
			utc_seconds time;
			friend auto operator<=>(const lesson_refresh&, const lesson_refresh&) = default;
		};
	}

	class scheduler final: public drogon::Plugin< scheduler >, public std::enable_shared_from_this< scheduler >
	{
		trantor::EventLoop* loop_{};
		const time_zone* zone_{};
		std::unique_ptr< client > client_;
		days sync_days_;
		seconds retry_interval_;
		std::map< hours, trantor::TimerId > static_timers_;
		std::set< seconds > pre_lesson_offsets_;
		std::map< lesson_refresh, trantor::TimerId > lesson_timers_;
		std::deque< sync_job > queue_;
		std::set< sync_job > pending_;
		bool loading_calendars_{};
		bool draining_{};
		bool running_{};

		void read_sync_config(const Json::Value& config);
		local_days local_day(utc_seconds time) const;
		utc_seconds to_utc(local_seconds time) const;
		void schedule_static(hours hour);
		void start_full_sync();
		void schedule_lessons(calendar target, date_range range, const std::vector< schedule_entry >& entries);
		void refresh_lesson(lesson_refresh key);
		void enqueue(calendar target, date_range range);
		drogon::Task<> sync_all();
		drogon::Task<> drain_queue();
		drogon::Task< std::vector< schedule_entry > > fetch_schedule(
				std::int64_t group_id, utc_seconds from, utc_seconds to);
		drogon::Task< bool > persist(calendar target, date_range range, const std::vector< schedule_entry >& entries);
		drogon::Task<> refresh(sync_job job);

	public:
		void initAndStart(const Json::Value& config) override;
		void shutdown() override;
		void sync_calendar(calendar target);
		drogon::Task< Json::Value > search_groups(std::string name);
	};

	void scheduler::initAndStart(const Json::Value&)
	{
		const auto& config = drogon::app().getCustomConfig()["sources"]["ruz"];
		const auto base_url = config.get("base_url", "https://ruz.spbstu.ru");
		const auto zone = config.get("timezone", "Europe/Moscow");
		const auto startup = config.get("sync_on_startup", true);
		if (!base_url.isString() || !zone.isString() || !startup.isBool()
				|| (!base_url.asString().starts_with("https://") && !base_url.asString().starts_with("http://")))
			throw std::invalid_argument("invalid RUZ configuration: base_url, timezone or sync_on_startup");
		zone_ = locate_zone(zone.asString());
		sync_days_ = days{config_integer(config, "sync_days", 7, 90)};
		retry_interval_ = seconds{config_integer(config, "retry_interval_seconds", 30, 3600)};
		read_sync_config(config["sync"]);
		loop_ = drogon::app().getLoop();
		client_ = std::make_unique< client >(base_url.asString(), loop_,
				duration< double >{config_number(config, "min_request_interval_seconds", 1.0, 3600.0)},
				config_number(config, "request_timeout_seconds", 10.0, 120.0), retry_interval_);
		running_ = true;
		for (const auto& [hour, timer]: static_timers_)
			schedule_static(hour);
		if (startup.asBool())
			loop_->queueInLoop([weak = weak_from_this()]
			{
				if (const auto self = weak.lock())
					self->start_full_sync();
			});
	}

	void scheduler::read_sync_config(const Json::Value& config)
	{
		if (!config.isObject() || !config["static"].isArray() || !config["pre_lesson"].isArray())
			throw std::invalid_argument("invalid RUZ configuration: sync.static and sync.pre_lesson must be arrays");
		for (const auto& value: config["static"])
		{
			if (!value.isInt() || value.asInt() < 0 || value.asInt() > 23)
				throw std::invalid_argument("invalid RUZ configuration: sync.static hours must be integers from 0 to 23");
			static_timers_.try_emplace(hours{value.asInt()});
		}
		for (const auto& value: config["pre_lesson"])
		{
			if (!value.isNumeric() || !std::isfinite(value.asDouble()) || value.asDouble() < 0 || value.asDouble() > 24)
				throw std::invalid_argument("invalid RUZ configuration: sync.pre_lesson hours must be from 0 to 24");
			pre_lesson_offsets_.insert(round< seconds >(duration< double, std::ratio< 3600 > >{value.asDouble()}));
		}
	}

	void scheduler::shutdown()
	{
		running_ = false;
		if (!loop_)
			return;
		for (const auto& [hour, timer]: static_timers_)
			loop_->invalidateTimer(timer);
		for (const auto& [key, timer]: lesson_timers_)
			loop_->invalidateTimer(timer);
	}

	local_days scheduler::local_day(utc_seconds time) const
	{
		return floor< days >(zone_->to_local(utc_clock::to_sys(time)));
	}

	utc_seconds scheduler::to_utc(local_seconds time) const
	{
		return utc_clock::from_sys(zone_->to_sys(time, choose::earliest));
	}

	void scheduler::schedule_static(hours hour)
	{
		const auto now = floor< seconds >(utc_clock::now());
		auto local = local_day(now) + hour;
		if (to_utc(local) <= now)
			local += days{1};
		static_timers_[hour] = loop_->runAt(timer_date(to_utc(local)), [weak = weak_from_this(), hour]
		{
			if (const auto self = weak.lock(); self && self->running_)
			{
				self->schedule_static(hour);
				self->start_full_sync();
			}
		});
	}

	void scheduler::start_full_sync()
	{
		drogon::async_run([self = shared_from_this()] { return self->sync_all(); });
	}

	void scheduler::enqueue(calendar target, date_range range)
	{
		const sync_job job{target, to_utc(range.from), to_utc(range.to)};
		if (!running_ || !pending_.insert(job).second)
			return;
		queue_.push_back(job);
		if (!draining_)
			drogon::async_run([self = shared_from_this()] { return self->drain_queue(); });
	}

	void scheduler::sync_calendar(calendar target)
	{
		const auto today = local_day(floor< seconds >(utc_clock::now()));
		enqueue(target, {today, today + sync_days_});
	}

	drogon::Task< Json::Value > scheduler::search_groups(std::string name)
	{
		co_return co_await client_->search_groups(std::move(name));
	}

	drogon::Task<> scheduler::sync_all()
	{
		if (!running_ || loading_calendars_)
			co_return;
		loading_calendars_ = true;
		for (int attempt = 0; attempt < 2 && running_; ++attempt)
		{
			try
			{
				const auto calendars = co_await load_calendars();
				co_await drogon::switchThreadCoro(loop_);
				const auto today = local_day(floor< seconds >(utc_clock::now()));
				for (const auto& target: calendars)
					enqueue(target, {today, today + sync_days_});
				break;
			}
			catch (const drogon::orm::DrogonDbException& error)
			{
				LOG_ERROR << "Cannot load RUZ calendars: " << error.base().what();
			}
			catch (const std::exception& error)
			{
				LOG_ERROR << "Cannot load RUZ calendars: " << error.what();
			}
			co_await drogon::switchThreadCoro(loop_);
			if (attempt == 0 && running_)
				co_await drogon::sleepCoro(loop_, retry_interval_);
		}
		loading_calendars_ = false;
	}

	drogon::Task<> scheduler::drain_queue()
	{
		draining_ = true;
		while (running_ && !queue_.empty())
		{
			const auto job = queue_.front();
			queue_.pop_front();
			try
			{
				co_await refresh(job);
			}
			catch (const std::exception& error)
			{
				LOG_WARN << "Cannot sync RUZ calendar " << job.target.id << ": " << error.what();
			}
			pending_.erase(job);
		}
		draining_ = false;
	}

	drogon::Task< std::vector< schedule_entry > > scheduler::fetch_schedule(
			std::int64_t group_id, utc_seconds from, utc_seconds to)
	{
		std::vector< schedule_entry > result;
		const auto last = local_day(to);
		for (auto first = local_day(from); first < last; )
		{
			const auto monday = first - (weekday{first} - Monday);
			const auto end = (std::min)(monday + weeks{1}, last);
			const auto payload = co_await client_->fetch_week(group_id, first);
			auto entries = read_schedule(payload, *zone_, {first, end});
			result.insert(result.end(), std::make_move_iterator(entries.begin()), std::make_move_iterator(entries.end()));
			first = end;
		}
		co_return result;
	}

	drogon::Task< bool > scheduler::persist(calendar target, date_range range, const std::vector< schedule_entry >& entries)
	{
		for (int attempt = 0; attempt < 2 && running_; ++attempt)
		{
			try
			{
				co_await save_schedule(target, range, entries);
				co_await drogon::switchThreadCoro(loop_);
				co_return true;
			}
			catch (const drogon::orm::DrogonDbException& error)
			{
				LOG_ERROR << "Cannot save RUZ calendar " << target.id << ": " << error.base().what();
			}
			catch (const std::exception& error)
			{
				LOG_ERROR << "Cannot save RUZ calendar " << target.id << ": " << error.what();
			}
			co_await drogon::switchThreadCoro(loop_);
			if (attempt == 0 && running_)
				co_await drogon::sleepCoro(loop_, retry_interval_);
		}
		co_return false;
	}

	drogon::Task<> scheduler::refresh(sync_job job)
	{
		const auto entries = co_await fetch_schedule(job.target.group_id, job.from, job.to);
		const date_range range{local_day(job.from), local_day(job.to)};
		if (co_await persist(job.target, range, entries) && running_)
			schedule_lessons(job.target, range, entries);
	}

	void scheduler::schedule_lessons(calendar target, date_range range, const std::vector< schedule_entry >& entries)
	{
		std::set< lesson_refresh > wanted;
		const auto now = utc_clock::now();
		for (const auto& entry: entries)
		{
			for (const auto offset: pre_lesson_offsets_)
			{
				const auto time = entry.start - offset;
				if (time > now)
					wanted.insert({target, local_day(entry.start), time});
			}
		}
		for (auto timer = lesson_timers_.begin(); timer != lesson_timers_.end(); )
		{
			const auto& key = timer->first;
			if (key.target == target && key.date >= range.from && key.date < range.to && !wanted.contains(key))
			{
				loop_->invalidateTimer(timer->second);
				timer = lesson_timers_.erase(timer);
			}
			else
				++timer;
		}
		for (const auto& key: wanted)
		{
			if (!lesson_timers_.contains(key))
				lesson_timers_[key] = loop_->runAt(timer_date(key.time), [weak = weak_from_this(), key]
				{
					if (const auto self = weak.lock(); self && self->running_)
						self->refresh_lesson(key);
				});
		}
	}

	void scheduler::refresh_lesson(lesson_refresh key)
	{
		if (lesson_timers_.erase(key) != 0)
			enqueue(key.target, {key.date, key.date + days{1}});
	}

	namespace
	{
		std::shared_ptr< scheduler > get_scheduler()
		{
			if (!drogon::app().isRunning())
				return {};
			return std::dynamic_pointer_cast< scheduler >(drogon::app().getSharedPlugin(scheduler::classTypeName()));
		}
	}

	drogon::Task< Json::Value > search_groups(std::string name)
	{
		co_await drogon::switchThreadCoro(drogon::app().getLoop());
		const auto plugin = get_scheduler();
		if (!plugin)
			throw std::runtime_error("RUZ plugin is disabled");
		co_return co_await plugin->search_groups(std::move(name));
	}

	void sync_calendar(std::int64_t calendar_id, std::int64_t group_id)
	{
		drogon::app().getLoop()->queueInLoop([calendar_id, group_id]
		{
			if (const auto plugin = get_scheduler())
				plugin->sync_calendar({calendar_id, group_id});
		});
	}
}
