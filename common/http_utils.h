#pragma once

#include <drogon/drogon.h>

namespace api
{
	void send_400(drogon::AdviceCallback&& callback, std::string errcode, std::string errmsg)
	{
		Json::Value json, err;
		err["code"] = errcode;
		err["msg"] = errmsg;
		json["error"] = std::move(err);
		auto response = drogon::HttpResponse::newHttpJsonResponse(std::move(json));
		response->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
		callback(response);
	}
}
