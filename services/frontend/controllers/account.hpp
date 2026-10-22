#pragma once

#include <drogon/HttpController.h>
#include <http_utils.hpp>

namespace api::frontend::ctrl
{
	class account: public drogon::HttpController< account >
	{
	public:
		account();

		static void initPathRouting();

		void index(const api::request_t& req, api::response_callback&& callback);
		void login(const api::request_t& req, api::response_callback&& callback);
		void register_p(const api::request_t& req, api::response_callback&& callback);

	private:
		std::string sessid_key;
	};
}
