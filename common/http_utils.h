#pragma once

#include <drogon/drogon.h>

namespace api
{
	namespace response
	{
		drogon::HttpResponsePtr new_response(drogon::HttpStatusCode code = drogon::k204NoContent);
		drogon::HttpResponsePtr new_error(drogon::HttpStatusCode code,
					Json::StaticString errcode, Json::StaticString errmsg);
		drogon::HttpResponsePtr new_error(drogon::HttpStatusCode code,
					const std::string& errcode, const std::string& errmsg);
		drogon::HttpResponsePtr new_400(Json::StaticString errcode, Json::StaticString errmsg);
		drogon::HttpResponsePtr new_400(const std::string& errcode, const std::string& errmsg);

		drogon::HttpResponsePtr new_json(const Json::Value& json, drogon::HttpStatusCode code = drogon::k200OK);
		drogon::HttpResponsePtr new_json(Json::Value&& json, drogon::HttpStatusCode code = drogon::k200OK);
	}

	Json::Value prepare_array(const std::shared_ptr< std::vector< char > >& arr);
	std::vector< std::size_t > prepare_vector(const std::shared_ptr< std::vector< char > >& arr);
	std::vector< char > parse_array(const Json::Value& arr);
	void parse_vector(const std::shared_ptr< std::vector< char > >& result, const std::vector< std::size_t >& arr);

	Json::StaticString operator""_s(const char* str, size_t);
}
