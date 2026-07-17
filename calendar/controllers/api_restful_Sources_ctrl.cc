#include "api_restful_Sources_ctrl.h"

#include <string>

void api::restful::Sources_ctrl::getOne(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Sources::PrimaryKeyType&& id)
{
	Sources_ctrlBase::getOne(req, std::move(callback), std::move(id));
}
void api::restful::Sources_ctrl::updateOne(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Sources::PrimaryKeyType&& id)
{
	Sources_ctrlBase::updateOne(req, std::move(callback), std::move(id));
}
void api::restful::Sources_ctrl::deleteOne(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Sources::PrimaryKeyType&& id)
{
	Sources_ctrlBase::deleteOne(req, std::move(callback), std::move(id));
}

void api::restful::Sources_ctrl::get(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
	Sources_ctrlBase::get(req, std::move(callback));
}
void api::restful::Sources_ctrl::create(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
	Sources_ctrlBase::create(req, std::move(callback));
}
