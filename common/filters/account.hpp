#pragma once

#include <drogon/HttpMiddleware.h>
#include <http_utils.hpp>

namespace api::middlware
{
	class logged: public middlware_t< logged >
	{
	public:
		void invoke(const request_t& req, middlware_pass_callback&& pass,
					middlware_reject_callback&& reject) override;
	private:
		response_t k401_unauthorized = response::new_401_unauthorized();
	};

	class admin: public middlware_t< admin >
	{
	public:
		void invoke(const request_t& req, middlware_pass_callback&& pass,
					middlware_reject_callback&& reject) override;
	private:
		response_t k403_forbidden = response::new_403_forbidden();
	};
}
