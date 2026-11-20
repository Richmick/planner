#include "ruz_schedule.hpp"

#include <drogon/utils/Utilities.h>
#include <trantor/utils/Utilities.h>

#include <algorithm>
#include <format>
#include <map>
#include <sstream>
#include <stdexcept>

namespace api::calendars::ruz
{
	namespace
	{
		using namespace std::chrono;

		template< typename Time >
		Time parse_time(std::string value, const char* format)
		{
			std::istringstream input(std::move(value));
			Time result;
			from_stream(input, format, result);
			if (input.fail() || input.peek() != std::char_traits< char >::eof())
				throw std::runtime_error("invalid date or time in RUZ schedule");
			return result;
		}

		std::string field(const Json::Value& item, const char* key)
		{
			const auto& value = item[key];
			return value.isString() || value.isIntegral() ? value.asString() : std::string{};
		}

		std::string key_for(const Json::Value& value)
		{
			const auto hash = trantor::utils::sha256(value.toStyledString());
			return trantor::utils::toHexString(hash.bytes, 16);
		}

		Json::Value participants(const Json::Value& values)
		{
			Json::Value result(Json::arrayValue);
			if (!values.isArray())
				return result;

			std::vector< std::string > keys;
			for (const auto& value: values)
			{
				if (!value.isObject())
					continue;
				const auto id = field(value, "id");
				keys.push_back(id.empty() ? field(value, "name") : id);
			}
			std::sort(keys.begin(), keys.end());
			for (const auto& key: keys)
				result.append(key);
			return result;
		}

		std::string identity(const std::string& date, const Json::Value& lesson)
		{
			Json::Value key;
			key["date"] = date;
			key["groups"] = participants(lesson["groups"]);
			key["subgroup"] = lesson["subgroup"];
			key["subgroups"] = lesson["subgroups"];
			const auto id = field(lesson, "id");
			if (!id.empty())
				key["id"] = id;
			else
			{
				key["start"] = field(lesson, "time_start");
				key["subject"] = field(lesson, "subject");
				key["type"] = lesson["type"];
			}
			return key_for(key);
		}

		utc_seconds parse_lesson_time(const std::string& date, const std::string& time, const time_zone& zone)
		{
			const auto format = time.find(':') == time.rfind(':') ? "%F %R" : "%F %T";
			const auto local = parse_time< local_seconds >(date + ' ' + time, format);
			return utc_clock::from_sys(zone.to_sys(local, choose::earliest));
		}

		schedule_entry read_lesson(const std::string& date, const Json::Value& lesson, const time_zone& zone)
		{
			if (!lesson.isObject())
				throw std::runtime_error("invalid lesson in RUZ schedule");
			schedule_entry result;
			result.name = field(lesson, "subject");
			result.start = parse_lesson_time(date, field(lesson, "time_start"), zone);
			result.end = parse_lesson_time(date, field(lesson, "time_end"), zone);
			if (result.name.empty() || result.end <= result.start)
				throw std::runtime_error("invalid lesson interval or subject in RUZ schedule");
			result.external_id = identity(date, lesson);
			result.source["date"] = date;
			result.source["lesson"] = lesson;
			return result;
		}

		void append_lessons(std::map< std::string, std::vector< schedule_entry > >& entries,
				const Json::Value& day, const time_zone& zone, date_range range)
		{
			if (!day.isObject() || !day["lessons"].isArray())
				throw std::runtime_error("invalid day in RUZ schedule");
			const auto date = field(day, "date");
			const auto local_day = parse_time< local_days >(date, "%F");
			if (local_day < range.from || local_day >= range.to)
				return;
			for (const auto& lesson: day["lessons"])
			{
				auto item = read_lesson(date, lesson, zone);
				entries[item.external_id].push_back(std::move(item));
			}
		}

		void copy_distinct(std::vector< schedule_entry >& result, std::vector< schedule_entry >& entries)
		{
			std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right)
			{
				return left.source < right.source;
			});
			const auto end = std::unique(entries.begin(), entries.end(), [](const auto& left, const auto& right)
			{
				return left.source == right.source;
			});
			for (auto item = entries.begin(); item != end; ++item)
			{
				if (item != entries.begin())
					item->external_id = key_for(item->source);
				result.push_back(std::move(*item));
			}
		}
	}

	std::string format_date(std::chrono::local_days date)
	{
		return std::format("{:%F}", std::chrono::year_month_day{date});
	}

	std::string format_utc(std::chrono::utc_seconds time)
	{
		return std::format("{:%FT%TZ}", time);
	}

	std::vector< schedule_entry > read_schedule(const Json::Value& payload,
			const std::chrono::time_zone& zone, date_range range)
	{
		if (!payload.isObject() || !payload["days"].isArray())
			throw std::runtime_error("RUZ schedule does not contain days");
		std::map< std::string, std::vector< schedule_entry > > entries;
		for (const auto& day: payload["days"])
			append_lessons(entries, day, zone, range);

		std::vector< schedule_entry > result;
		for (auto& [key, group]: entries)
			copy_distinct(result, group);
		return result;
	}

	std::string legacy_external_id(const Json::Value& source)
	{
		if (!source.isObject() || !source["lesson"].isObject() || !source["date"].isString())
			return {};
		return identity(source["date"].asString(), source["lesson"]);
	}
}
