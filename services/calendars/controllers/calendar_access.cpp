#include "calendars_ctrl.hpp"

#include "database_utils.hpp"

namespace api::calendars
{
	namespace
	{
		enum class access_level { none, read, write, share };
		enum class grant_result { not_found, forbidden, granted };

		constexpr std::string_view access_query =
				" FROM calendars c LEFT JOIN calendar_access a ON a.calendar_id = c.id AND a.user_id = ?"
				" WHERE c.id = ?";

		access_level level(std::string_view access)
		{
			if (access == "r") return access_level::read;
			if (access == "w") return access_level::write;
			if (access == "s") return access_level::share;
			return access_level::none;
		}

		bool is_owner(const drogon::orm::Row& calendar, std::int64_t user_id)
		{
			return !calendar["owner_id"].isNull() && calendar["owner_id"].as< std::int64_t >() == user_id;
		}

		drogon::Task< grant_result > save_grant(std::int64_t calendar_id, std::int64_t actor_id,
				std::int64_t user_id, const std::string& access)
		{
			using drogon::orm::TransactionType;
			const auto database = drogon::app().getDbClient();
			const bool postgresql = database->type() == drogon::orm::ClientType::PostgreSQL;
			const auto calendar_query = db::query(postgresql
						? "SELECT visibility, owner_id FROM calendars WHERE id = ? FOR UPDATE"
						: "SELECT visibility, owner_id FROM calendars WHERE id = ?", postgresql);
			const auto actor_query = db::query(postgresql
						? "SELECT access FROM calendar_access WHERE calendar_id = ? AND user_id = ? FOR SHARE"
						: "SELECT access FROM calendar_access WHERE calendar_id = ? AND user_id = ?", postgresql);
			const auto grant_query = db::query(
					"INSERT INTO calendar_access (calendar_id, user_id, access) VALUES (?, ?, ?)"
					" ON CONFLICT (calendar_id, user_id) DO UPDATE SET access = excluded.access", postgresql);
			const auto subscribe_query = db::query(
					"INSERT INTO subscriptions (calendar_id, user_id) VALUES (?, ?)"
					" ON CONFLICT (user_id, calendar_id) DO NOTHING", postgresql);

			auto transaction = co_await database->newTransactionCoro(
					postgresql ? TransactionType::Deferred : TransactionType::Immediate);
			try
			{
				const auto rows = co_await transaction->execSqlCoro(calendar_query, calendar_id);
				if (rows.empty()) co_return grant_result::not_found;
				const auto calendar = rows.front();
				const std::string visibility = calendar["visibility"].as< std::string >();
				const bool owner = is_owner(calendar, actor_id);
				if (visibility == "private" && !owner) co_return grant_result::not_found;
				if (visibility != "protected") co_return grant_result::forbidden;
				if (!owner)
				{
					const auto permissions = co_await transaction->execSqlCoro(actor_query, calendar_id, actor_id);
					if (permissions.empty() || level(permissions.front()["access"].as< std::string >()) != access_level::share)
						co_return grant_result::forbidden;
				}
				co_await transaction->execSqlCoro(grant_query, calendar_id, user_id, access);
				co_await transaction->execSqlCoro(subscribe_query, calendar_id, user_id);
			}
			catch (...)
			{
				transaction->rollback();
				throw;
			}
			co_await db::commit(transaction);
			co_return grant_result::granted;
		}
	}

	drogon::Task<> calendars_ctrl::get_calendar(api::request_t request, api::response_callback callback,
			std::string calendar_id)
	{
		const std::int64_t user_id = parse_id(request->getHeader("user-id"));
		if (user_id == -1) co_return callback(resp.k401_unauthorized);
		const std::int64_t id = parse_id(calendar_id);
		if (id == -1) co_return callback(resp.k400_invalid_calendar_id);

		try
		{
			const auto database = drogon::app().getDbClient();
			const bool postgresql = database->type() == drogon::orm::ClientType::PostgreSQL;
			std::string sql =
					"SELECT c.id, c.type_id, c.external_id, c.metadata, c.visibility, c.owner_id, a.access, ";
			sql += calendar_created_at_sql(postgresql);
			sql += " AS created_at";
			sql += access_query;
			const auto rows = co_await database->execSqlCoro(db::query(sql, postgresql), user_id, id);
			if (rows.empty()) co_return callback(resp.k404_not_found);
			const auto calendar = rows.front();
			const std::string visibility = calendar["visibility"].as< std::string >();
			const bool owner = is_owner(calendar, user_id);
			if (visibility == "private" && !owner) co_return callback(resp.k404_not_found);
			if (visibility == "protected" && !owner && calendar["access"].isNull())
				co_return callback(resp.k403_forbidden);
			co_return callback(api::response::new_json(calendar_json(calendar)));
		}
		catch (const drogon::orm::DrogonDbException& error)
		{
			co_return callback(database_error(error));
		}
	}

	drogon::Task<> calendars_ctrl::grant_calendar_access(api::request_t request, api::response_callback callback,
			std::string calendar_id)
	{
		const std::int64_t actor_id = parse_id(request->getHeader("user-id"));
		if (actor_id == -1) co_return callback(resp.k401_unauthorized);
		const std::int64_t id = parse_id(calendar_id);
		if (id == -1) co_return callback(resp.k400_invalid_calendar_id);
		const auto& body = request->getJsonObject();
		if (!body || !body->isObject()) co_return callback(resp.k400_invalid_body);
		const Json::Value& data = *body;
		const std::int64_t user_id = parse_json_id(data["user_id"]);
		if (user_id == -1) co_return callback(resp.k400_invalid_user_id);
		const std::string access = data["access"].isString() ? data["access"].asString() : std::string{};
		if (level(access) == access_level::none) co_return callback(resp.k400_invalid_access);

		try
		{
			const grant_result result = co_await save_grant(id, actor_id, user_id, access);
			if (result == grant_result::not_found) co_return callback(resp.k404_not_found);
			if (result == grant_result::forbidden) co_return callback(resp.k403_forbidden);
			co_return callback(resp.k204_access_granted);
		}
		catch (const drogon::orm::DrogonDbException& error)
		{
			co_return callback(database_error(error));
		}
		catch (const std::exception& error)
		{
			co_return callback(database_error(error));
		}
	}

	drogon::Task<> calendars_ctrl::request_calendar_access(api::request_t request, api::response_callback callback)
	{
		const std::int64_t actor_id = parse_id(request->getHeader("user-id"));
		if (actor_id == -1) co_return callback(resp.k401_unauthorized);
		const auto& body = request->getJsonObject();
		if (!body || !body->isObject()) co_return callback(resp.k400_invalid_body);
		const Json::Value& data = *body;
		if (!data["event"].isString() || data["event"].asString() != "access-request")
			co_return callback(resp.k400_invalid_access_event);
		const std::int64_t user_id = parse_json_id(data["user_id"]);
		if (user_id == -1) co_return callback(resp.k400_invalid_user_id);
		if (user_id != actor_id) co_return callback(resp.k403_forbidden);
		const std::int64_t calendar_id = parse_json_id(data["calendar_id"]);
		if (calendar_id == -1) co_return callback(resp.k400_invalid_calendar_id);
		const std::string access = data["access"].isString() ? data["access"].asString() : std::string{};
		const access_level requested = level(access);
		if (requested == access_level::none) co_return callback(resp.k400_invalid_access);

		try
		{
			const auto database = drogon::app().getDbClient();
			const bool postgresql = database->type() == drogon::orm::ClientType::PostgreSQL;
			std::string sql = "SELECT c.visibility, c.owner_id, a.access";
			sql += access_query;
			const auto rows = co_await database->execSqlCoro(db::query(sql, postgresql), user_id, calendar_id);
			if (rows.empty()) co_return callback(resp.k404_not_found);
			const auto calendar = rows.front();
			const std::string visibility = calendar["visibility"].as< std::string >();
			const bool owner = is_owner(calendar, user_id);
			if (visibility == "private" && !owner) co_return callback(resp.k404_not_found);
			if (visibility != "protected") co_return callback(resp.k403_forbidden);
			if (owner || level(calendar["access"].as< std::string >()) >= requested)
				co_return callback(resp.k409_access_already_granted);
		}
		catch (const drogon::orm::DrogonDbException& error)
		{
			co_return callback(database_error(error));
		}
		co_return callback(co_await notifier.send_access_request(calendar_id, user_id, access));
	}
}
