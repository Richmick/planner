#include "account.hpp"

api::frontend::ctrl::account::account()
{
	sessid_key = drogon::app().getCustomConfig().get("session_cookie_key", "sessid").asString();
}
void api::frontend::ctrl::account::initPathRouting()
{
	ADD_METHOD_TO(account::index, "/", drogon::Get);
	ADD_METHOD_TO(account::login, "/login", drogon::Get);
	ADD_METHOD_TO(account::register_p, "/register", drogon::Get);
}

void api::frontend::ctrl::account::index(const api::request_t& req, api::response_callback&& callback)
{
	drogon::HttpViewData data;
	data.insert("sessid", req->getCookie(sessid_key));
	callback(drogon::HttpResponse::newHttpViewResponse("views/index.csp", data));
}
void api::frontend::ctrl::account::login(const api::request_t& req, api::response_callback&& callback)
{
	drogon::HttpViewData data;
	data.insert("sessid", req->getCookie(sessid_key));
	callback(drogon::HttpResponse::newHttpViewResponse("views/account/login.csp"));
}
void api::frontend::ctrl::account::register_p(const api::request_t& req, api::response_callback&& callback)
{
	drogon::HttpViewData data;
	data.insert("sessid", req->getCookie(sessid_key));
	callback(drogon::HttpResponse::newHttpViewResponse("views/account/register_p.csp"));
}
