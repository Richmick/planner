#include "api_restful_Users_ctrl.h"

#include <string>
#include <sodium.h>

void api::restful::Users_ctrl::getOne(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Users::PrimaryKeyType&& id)
{
	Users_ctrlBase::getOne(req, std::move(callback), std::move(id));
}
void api::restful::Users_ctrl::updateOne(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Users::PrimaryKeyType&& id)
{
	Users_ctrlBase::updateOne(req, std::move(callback), std::move(id));
}
void api::restful::Users_ctrl::deleteOne(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Users::PrimaryKeyType&& id)
{
	Users_ctrlBase::deleteOne(req, std::move(callback), std::move(id));
}

void api::restful::Users_ctrl::get(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
	Users_ctrlBase::get(req, std::move(callback));
}
void api::restful::Users_ctrl::create(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
	Users_ctrlBase::create(req, std::move(callback));
}
void api::restful::Users_ctrl::login(const drogon::HttpRequestPtr& request, drogon::AdviceCallback&& callback)
{
	const std::shared_ptr< Json::Value >& body = request->getJsonObject();
	if (!body || !body->isObject()) {}
	else
	{
		std::string nickname = body->get("nickname", "").asString();
		std::string password = body->get("password", "").asString();
		if (!nickname.empty() && !password.empty())
		{
			drogon::orm::DbClientPtr sql_client = getDbClient();
	
			drogon::orm::Mapper< Users > mapper(sql_client);
			try
			{
				Users user = mapper.findOne(drogon::orm::Criteria{Users::Cols::_nickname,
							drogon::orm::CompareOperator::EQ, nickname});
				if (const auto& user_password = user.getPassword();
							user_password && crypto_pwhash_str_verify(user_password->c_str(),
								password.c_str(), password.length()) == 0)
				{
					Json::Value result = user.toJson();
					result.removeMember(Users::Cols::_password);
					auto subscribes = user.getSubscribes();
					if (subscribes)
					{
						Json::Value arr{Json::arrayValue};
						std::size_t* p = reinterpret_cast< std::size_t* >(subscribes->data());
						std::size_t* end = reinterpret_cast< std::size_t* >(subscribes->data())
								+ (subscribes->size() / sizeof std::size_t);
						for (; p < end; p++) arr.append(*p);
						result[Users::Cols::_subscribes] = std::move(arr);
					}
					callback(drogon::HttpResponse::newHttpJsonResponse(result));
					return;
				}
			}
			catch (const drogon::orm::UnexpectedRows&)
			{}
			auto response = drogon::HttpResponse::newHttpJsonResponse("wrong login or password");
			response->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
			callback(response);
			return;
		}
	}
	auto response = drogon::HttpResponse::newHttpJsonResponse("missing login or password in json");
	response->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
	callback(response);
}
