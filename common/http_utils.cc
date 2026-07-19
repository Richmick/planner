#include "http_utils.h"

void api::send_code_only(drogon::AdviceCallback&& callback, drogon::HttpStatusCode code)
{
	auto response = drogon::HttpResponse::newHttpResponse();
	response->setStatusCode(code);
	callback(response);
}
void api::send_success(drogon::AdviceCallback&& callback)
{
	send_code_only(std::move(callback), drogon::HttpStatusCode::k204NoContent);
}
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

Json::Value api::prepare_array(const std::shared_ptr< std::vector< char > >& arr)
{
	if (!arr) return Json::arrayValue;
	Json::Value result{Json::arrayValue};
	std::size_t* read_p = reinterpret_cast< std::size_t* >(arr->data());
	std::size_t* end = read_p + (arr->size() / sizeof std::size_t);
	for (; read_p < end; read_p++) result.append(*read_p);
	return result;
}
std::string api::parse_array(const Json::Value& arr)
{
	if (!arr.isArray()) return {};
	std::string result;
	result.resize(arr.size() * sizeof std::size_t);

	std::size_t* write_p = reinterpret_cast< std::size_t* >(result.data());
	std::size_t* start_p = write_p;
	for (const auto& i : arr) *(write_p++) = i.asUInt64();

	std::sort(start_p, write_p);
	result.resize((std::unique(start_p, write_p) - start_p) * sizeof std::size_t);
	return result;
}

drogon::HttpResponsePtr api::new_json_response(const Json::Value& json, drogon::HttpStatusCode code)
{
	drogon::HttpResponsePtr result = drogon::HttpResponse::newHttpJsonResponse(json);
	result->setStatusCode(code);
	return result;
}
drogon::HttpResponsePtr api::new_json_response(Json::Value&& json, drogon::HttpStatusCode code)
{
	drogon::HttpResponsePtr result = drogon::HttpResponse::newHttpJsonResponse(std::move(json));
	result->setStatusCode(code);
	return result;
}
