#include <drogon/drogon.h>
#include <data/user.h>

using callback_t = std::function< void(const drogon::HttpResponsePtr&) >;

drogon::Task<> test_handler(drogon::HttpRequestPtr request, callback_t callback)
{
	auto user = request->session()->getOptional< data::user_t >("user");
	if (user)
	{
		auto sql_client = drogon::app().getDbClient();
		try
		{
			drogon::orm::Result result = co_await sql_client->execSqlCoro("SELECT COUNT(*) FROM users;");
			size_t num_users = result[0][0].as< std::size_t >();
			auto response = drogon::HttpResponse::newHttpResponse();
			response->setBody(std::to_string(num_users));
			callback(response);
		}
		catch(const drogon::orm::DrogonDbException &err)
		{
			auto response = drogon::HttpResponse::newHttpResponse();
			response->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
			response->setBody(err.base().what());
			callback(response);
		}
		/*auto config = drogon::app().getCustomConfig();
		auto response = drogon::HttpResponse::newHttpJsonResponse(config);
		response->addHeader("user", std::to_string(user->id));
		callback(response);*/
	}
	else
	{
		auto response = drogon::HttpResponse::newHttpResponse();
		response->setBody("<h1>401 unauthorized</h1><a href=\"login\">войти</a>");
		response->setStatusCode(drogon::HttpStatusCode::k401Unauthorized);
		callback(response);
	}
	co_return;
}

int main() {
	if (sodium_init() < 0)
	{
		std::cerr << "failed to initialize crypto library\n";
		return 1;
	}
	drogon::app().loadConfigFile("./config.json");
	drogon::app().registerHandler("/test", &test_handler, {drogon::Get});
	drogon::app().run();
	return 0;
}
