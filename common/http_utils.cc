#include "http_utils.h"

void api::send_error(drogon::AdviceCallback&& callback, drogon::HttpStatusCode code,
			Json::StaticString errcode, Json::StaticString errmsg)
{
	Json::Value json, err;
	err["code"] = errcode;
	err["msg"] = errmsg;
	json["error"] = std::move(err);
	auto response = drogon::HttpResponse::newHttpJsonResponse(std::move(json));
	response->setStatusCode(code);
	callback(response);
}
void api::send_error(drogon::AdviceCallback&& callback, drogon::HttpStatusCode code,
			const std::string& errcode, const std::string& errmsg)
{
	Json::Value json, err;
	err["code"] = errcode;
	err["msg"] = errmsg;
	json["error"] = std::move(err);
	auto response = drogon::HttpResponse::newHttpJsonResponse(std::move(json));
	response->setStatusCode(code);
	callback(response);
}
void api::send_400(drogon::AdviceCallback&& callback, const std::string& errcode, const std::string& errmsg)
{
	send_error(std::move(callback), drogon::HttpStatusCode::k400BadRequest, errcode, errmsg);
}
void api::send_400(drogon::AdviceCallback&& callback, Json::StaticString errcode, Json::StaticString errmsg)
{
	send_error(std::move(callback), drogon::HttpStatusCode::k400BadRequest, errcode, errmsg);
}
