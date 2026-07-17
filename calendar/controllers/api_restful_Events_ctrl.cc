#include "api_restful_Events_ctrl.h"

#include <string>

void api::restful::Events_ctrl::getOne(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
			Events::PrimaryKeyType&& id)
{
	Events_ctrlBase::getOne(req, std::move(callback), std::move(id));
}
void api::restful::Events_ctrl::updateOne(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
			Events::PrimaryKeyType&& id)
{
	Events_ctrlBase::updateOne(req, std::move(callback), std::move(id));
}
void api::restful::Events_ctrl::deleteOne(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback,
			Events::PrimaryKeyType&& id)
{
	Events_ctrlBase::deleteOne(req, std::move(callback), std::move(id));
}

void api::restful::Events_ctrl::get(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
	Events_ctrlBase::get(req, std::move(callback));
}
void api::restful::Events_ctrl::create(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
	Events_ctrlBase::create(req, std::move(callback));
}
