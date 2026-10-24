#pragma once

#include <version>
#if defined(__cpp_lib_expected) && __cpp_lib_expected > 0
	#include <expected>
#endif
#include <drogon/drogon.h>

namespace api
{
	namespace code
	{
		using type = drogon::HttpStatusCode;

		static constexpr inline type k200_ok = type::k200OK;
		static constexpr inline type k204_no_content = type::k204NoContent;

		static constexpr inline type k400_bad_request = type::k400BadRequest;
		static constexpr inline type k401_unauthorized = type::k401Unauthorized;
		static constexpr inline type k402_payment_required = type::k402PaymentRequired;
		static constexpr inline type k403_forbidden = type::k403Forbidden;
		static constexpr inline type k404_not_found = type::k404NotFound;
		static constexpr inline type k419_auth_timeout = static_cast<type>(419);
		static constexpr inline type k421_misdirected_request = type::k421MisdirectedRequest;

		static constexpr inline type k500_internal = type::k500InternalServerError;
		static constexpr inline type k502_bad_gateway = type::k502BadGateway;
		static constexpr inline type k504_timeout = type::k504GatewayTimeout;
		static constexpr inline bool ok(type c){ return (c >= 200) && (c < 300); }
	};
	using code_t = code::type;

	namespace response
	{
		using type = drogon::HttpResponsePtr;

		type new_response(code_t c = code::k204_no_content);

		type new_error(code_t code,
					Json::StaticString errcode, Json::StaticString errmsg);
		type new_error(code_t code,
					const std::string& errcode, const std::string& errmsg);
		type new_json(const Json::Value& json, code_t code = code::k200_ok);
		type new_json(Json::Value&& json, code_t code = code::k200_ok);
		type new_400(Json::StaticString errcode, Json::StaticString errmsg);
		type new_400(const std::string& errcode, const std::string& errmsg);

		type new_401_unauthorized();
		type new_403_forbidden();
		type new_404_not_found();
	};
	using response_t = response::type;
	using request_t = drogon::HttpRequestPtr;
	using response_callback = drogon::AdviceCallback;
	using request_callback = drogon::HttpReqCallback;

	template< class T >
	using middlware_t = drogon::HttpMiddleware< T >;
	using middlware_pass_callback = drogon::MiddlewareNextCallback;
	using middlware_reject_callback = drogon::MiddlewareCallback;

	#if defined(__cpp_lib_expected) && __cpp_lib_expected > 0
		template< class T >
		using expected = std::expected< T, std::reference_wrapper< const api::response_t > >;
		using unexpected = std::unexpected< std::reference_wrapper< const api::response_t > >;
	#endif

	std::string utc_now();

	Json::Value prepare_array(const std::shared_ptr< std::vector< char > >& arr);
	std::vector< std::size_t > prepare_vector(const std::shared_ptr< std::vector< char > >& arr);
	std::vector< char > parse_array(const Json::Value& arr);
	void parse_vector(const std::shared_ptr< std::vector< char > >& result, const std::vector< std::size_t >& arr);

	Json::StaticString operator""_s(const char* str, size_t);
}
