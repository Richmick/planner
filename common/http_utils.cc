#include "http_utils.h"

drogon::HttpResponsePtr api::response::new_response(drogon::HttpStatusCode code)
{
	auto response = drogon::HttpResponse::newHttpResponse();
	response->setStatusCode(code);
	return response;
}
drogon::HttpResponsePtr api::response::new_error(drogon::HttpStatusCode code,
			Json::StaticString errcode, Json::StaticString errmsg)
{
	Json::Value json, err;
	err["code"] = errcode;
	err["msg"] = errmsg;
	json["error"] = std::move(err);
	auto response = drogon::HttpResponse::newHttpJsonResponse(std::move(json));
	response->setStatusCode(code);
	return response;
}
drogon::HttpResponsePtr api::response::new_error(drogon::HttpStatusCode code,
			const std::string& errcode, const std::string& errmsg)
{
	Json::Value json, err;
	err["code"] = errcode;
	err["msg"] = errmsg;
	json["error"] = std::move(err);
	auto response = drogon::HttpResponse::newHttpJsonResponse(std::move(json));
	response->setStatusCode(code);
	return response;
}
drogon::HttpResponsePtr api::response::new_400(const std::string& errcode, const std::string& errmsg)
{
	return new_error(drogon::HttpStatusCode::k400BadRequest, errcode, errmsg);
}
drogon::HttpResponsePtr api::response::new_400(Json::StaticString errcode, Json::StaticString errmsg)
{
	return new_error(drogon::HttpStatusCode::k400BadRequest, errcode, errmsg);
}
drogon::HttpResponsePtr api::response::new_json(const Json::Value& json, drogon::HttpStatusCode code)
{
	drogon::HttpResponsePtr result = drogon::HttpResponse::newHttpJsonResponse(json);
	result->setStatusCode(code);
	return result;
}
drogon::HttpResponsePtr api::response::new_json(Json::Value&& json, drogon::HttpStatusCode code)
{
	drogon::HttpResponsePtr result = drogon::HttpResponse::newHttpJsonResponse(std::move(json));
	result->setStatusCode(code);
	return result;
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
std::vector< std::size_t > api::prepare_vector(const std::shared_ptr< std::vector< char > >& arr)
{
	std::vector< std::size_t > result;
	if (!arr) return result;
	std::size_t* read_p = reinterpret_cast< std::size_t* >(arr->data());
	std::size_t* end = read_p + (arr->size() / sizeof std::size_t);
	for (; read_p < end; read_p++) result.push_back(*read_p);
	return result;
}
std::vector< char > api::parse_array(const Json::Value& arr)
{
	std::vector< char > result;
	if (!arr.isArray()) return result;
	result.resize(arr.size() * sizeof std::size_t);

	std::size_t* write_p = reinterpret_cast< std::size_t* >(result.data());
	std::size_t* start_p = write_p;
	for (const auto& i: arr) *(write_p++) = i.asUInt64();

	std::sort(start_p, write_p);
	result.resize((std::unique(start_p, write_p) - start_p) * sizeof std::size_t);
	return result;
}
void api::parse_vector(const std::shared_ptr< std::vector< char > >& result, const std::vector< std::size_t >& arr)
{
	result->resize(arr.size() * sizeof std::size_t);
	std::size_t* write_p = reinterpret_cast< std::size_t* >(result->data());
	std::size_t* start_p = write_p;
	for (std::size_t i: arr) *(write_p++) = i;
}

Json::StaticString api::operator""_s(const char* str, size_t)
{
	return Json::StaticString{str};
}
