#pragma once

#include <drogon/drogon.h>

namespace api
{
	void send_code_only(drogon::AdviceCallback&& callback, drogon::HttpStatusCode code);
	void send_success(drogon::AdviceCallback&& callback);

	void send_error(drogon::AdviceCallback&& callback, drogon::HttpStatusCode code,
				Json::StaticString errcode, Json::StaticString errmsg);
	void send_400(drogon::AdviceCallback&& callback,
				Json::StaticString errcode, Json::StaticString errmsg);

	void send_error(drogon::AdviceCallback&& callback, drogon::HttpStatusCode code,
				const std::string& errcode, const std::string& errmsg);
	void send_400(drogon::AdviceCallback&& callback,
				const std::string& errcode, const std::string& errmsg);
}
