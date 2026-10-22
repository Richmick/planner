#include "account.hpp"

void api::middlware::logged::invoke(const request_t& req, middlware_pass_callback&& pass, middlware_reject_callback&& reject)
{
	if (req->getHeader("user-id").empty())
	{
		reject(k401_unauthorized);
		return;
	}
	pass(std::move(reject));
}
void api::middlware::admin::invoke(const request_t& req, middlware_pass_callback&& pass, middlware_reject_callback&& reject)
{
	if (req->getHeader("user-admin").empty())
	{
		reject(k403_forbidden);
		return;
	}
	pass(std::move(reject));
}
