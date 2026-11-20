#include "ruz_storage.hpp"

#include "database_utils.hpp"

#include <charconv>
#include <set>
#include <sstream>

namespace api::calendars::ruz
{
	namespace
	{
		drogon::Task<> upsert(drogon::orm::Transaction& transaction, calendar target,
				const schedule_entry& entry, bool postgresql)
		{
			Json::Value interval;
			interval["start"] = format_utc(entry.start);
			interval["end"] = format_utc(entry.end);
			Json::Value schedule(Json::arrayValue);
			schedule.append(interval);
			Json::StreamWriterBuilder writer;
			writer["indentation"] = "";
			co_await transaction.execSqlCoro(db::query(
					"INSERT INTO events (calendar_id, external_id, name, global_scheduled, source_data, finish_before) "
					"VALUES (?, ?, ?, ?{json}, ?{json}, ?) "
					"ON CONFLICT (calendar_id, external_id) DO UPDATE SET "
					"name = EXCLUDED.name, global_scheduled = EXCLUDED.global_scheduled, "
					"source_data = EXCLUDED.source_data, finish_before = EXCLUDED.finish_before, "
					"updated_at = CURRENT_TIMESTAMP, version = events.version + 1 "
					"WHERE events.name <> EXCLUDED.name OR events.global_scheduled <> EXCLUDED.global_scheduled "
					"OR events.source_data <> EXCLUDED.source_data OR events.finish_before IS NULL "
					"OR events.finish_before <> EXCLUDED.finish_before", postgresql),
					target.id, entry.external_id, entry.name, Json::writeString(writer, schedule),
					Json::writeString(writer, entry.source), interval["end"].asString());
		}

		std::string read_legacy_key(const std::string& text)
		{
			Json::CharReaderBuilder reader;
			std::istringstream input(text);
			Json::Value source;
			if (!Json::parseFromStream(reader, input, &source, nullptr))
				return {};
			return legacy_external_id(source);
		}

		drogon::Task<> reconcile_ids(drogon::orm::Transaction& transaction, calendar target,
				date_range range, const std::set< std::string >& incoming, bool postgresql)
		{
			const auto rows = co_await transaction.execSqlCoro(db::query(
					"SELECT id, external_id, source_data FROM events WHERE calendar_id = ? AND external_id IS NOT NULL "
					"AND (source_data->>'date') >= ? AND (source_data->>'date') < ? ORDER BY id", postgresql),
					target.id, format_date(range.from), format_date(range.to));
			std::set< std::string > existing;
			for (const auto& row: rows)
				existing.insert(row["external_id"].as< std::string >());

			for (const auto& row: rows)
			{
				const auto id = row["id"].as< std::int64_t >();
				const auto external_id = row["external_id"].as< std::string >();
				if (incoming.contains(external_id))
					continue;
				if (external_id.size() <= 16)
				{
					const auto key = read_legacy_key(row["source_data"].as< std::string >());
					if (incoming.contains(key) && existing.insert(key).second)
					{
						co_await transaction.execSqlCoro(db::query("UPDATE events SET external_id = ? WHERE id = ?",
								postgresql), key, id);
						continue;
					}
				}
			co_await transaction.execSqlCoro(db::query("DELETE FROM events WHERE id = ?", postgresql), id);
			}
		}
	}

	drogon::Task< std::vector< calendar > > load_calendars()
	{
		const auto database = drogon::app().getDbClient();
		const auto rows = co_await database->execSqlCoro(
				"SELECT id, external_id FROM calendars WHERE type_id = 'ruz' ORDER BY id");
		std::vector< calendar > result;
		for (const auto& row: rows)
		{
			const auto id = row["id"].as< std::int64_t >();
			const auto external_id = row["external_id"].as< std::string >();
			std::int64_t group = 0;
			const auto end = external_id.data() + external_id.size();
			const std::from_chars_result parsed = std::from_chars(external_id.data(), end, group);
			if (parsed.ec == std::errc{} && parsed.ptr == end && group > 0)
				result.push_back({id, group});
			else
				LOG_WARN << "Invalid RUZ group ID in calendar " << id;
		}
		co_return result;
	}

	drogon::Task<> save_schedule(calendar target, date_range range, const std::vector< schedule_entry >& entries)
	{
		std::set< std::string > incoming;
		for (const auto& entry: entries)
			incoming.insert(entry.external_id);

		const auto database = drogon::app().getDbClient();
		const bool postgresql = database->type() == drogon::orm::ClientType::PostgreSQL;
		auto transaction = co_await database->newTransactionCoro();
		try
		{
			co_await reconcile_ids(*transaction, target, range, incoming, postgresql);
			for (const auto& entry: entries)
				co_await upsert(*transaction, target, entry, postgresql);
		}
		catch (...)
		{
			transaction->rollback();
			throw;
		}
		co_await db::commit(transaction);
	}
}
