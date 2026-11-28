#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#include <drogon/drogon.h>

namespace api::calendars::db
{
	class commit: public drogon::CallbackAwaiter< bool >
	{
		std::shared_ptr< drogon::orm::Transaction > transaction_;

	public:
		explicit commit(std::shared_ptr< drogon::orm::Transaction >& transaction):
			transaction_(std::move(transaction))
		{}

		void await_suspend(std::coroutine_handle<> handle)
		{
			// Drogon submits COMMIT when the last transaction reference is released.
			auto transaction = std::move(transaction_);
			transaction->setCommitCallback([this, handle](bool committed)
			{
				setValue(committed);
				handle.resume();
			});
		}

		void await_resume()
		{
			if (!drogon::CallbackAwaiter< bool >::await_resume())
				throw std::runtime_error("Database commit failed");
		}
	};

	inline bool is_postgresql()
	{
		return drogon::app().getDbClient()->type() == drogon::orm::ClientType::PostgreSQL;
	}

	// Use ?{json} for JSON parameters. PostgreSQL needs an explicit JSONB cast.
	inline std::string query(std::string_view statement, bool postgresql)
	{
		std::string result;
		result.reserve(statement.size());
		unsigned parameter = 1;
		for (std::size_t index = 0; index < statement.size(); ++index)
		{
			if (statement[index] != '?')
			{
				result.push_back(statement[index]);
				continue;
			}

			const bool json = statement.substr(index, 7) == "?{json}";
			if (postgresql)
			{
				result.push_back('$');
				result += std::to_string(parameter++);
				if (json) result += "::jsonb";
			}
			else
			{
				result.push_back('?');
			}
			if (json) index += 6;
		}
		return result;
	}

	inline std::string query(std::string_view statement)
	{
		return query(statement, is_postgresql());
	}
}
