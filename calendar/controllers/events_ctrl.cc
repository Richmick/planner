#include "events_ctrl.h"

#include <string>

void api::restful::events_ctrl::get_one(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
			Events::PrimaryKeyType&& id)
{
	base_type::getOne(req, std::move(callback), std::move(id));
}
void api::restful::events_ctrl::update_one(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
			Events::PrimaryKeyType&& id)
{
	base_type::updateOne(req, std::move(callback), std::move(id));
}
void api::restful::events_ctrl::delete_one(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
			Events::PrimaryKeyType&& id)
{
	base_type::deleteOne(req, std::move(callback), std::move(id));
}

void api::restful::events_ctrl::get(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
	base_type::get(req, std::move(callback));
}
void api::restful::events_ctrl::create(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
	base_type::create(req, std::move(callback));
}
