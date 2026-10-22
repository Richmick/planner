#include "sources_ctrl.h"

#include <string>
#include <http_utils.h>

void api::restful::Sources_ctrl::get_one(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Sources::PrimaryKeyType&& id)
{
	base_type::getOne(req, std::move(callback), std::move(id));
}
void api::restful::Sources_ctrl::update_one(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Sources::PrimaryKeyType&& id)
{
	base_type::updateOne(req, std::move(callback), std::move(id));
}
void api::restful::Sources_ctrl::delete_one(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Sources::PrimaryKeyType&& id)
{
	base_type::deleteOne(req, std::move(callback), std::move(id));
}

void api::restful::Sources_ctrl::get(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
	base_type::get(req, std::move(callback));
}
void api::restful::Sources_ctrl::create(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
	base_type::create(req, std::move(callback));
}
void api::restful::Sources_ctrl::subscribe_user(const drogon::HttpRequestPtr& req,
			drogon::AdviceCallback&& callback, Sources::PrimaryKeyType&& id)
{
	using namespace api::response;
	using api::operator""_s;

	std::size_t user_id = static_cast< std::size_t >(id);
	const auto& json = req->getJsonObject();
	if (!json || !json->isObject())
		return callback(new_400("NOT_JSON"_s, "expected json object"_s));
	Json::Value create_list = json->get("create", Json::arrayValue);
	Json::Value delete_list = json->get("delete", Json::arrayValue);
	if (!create_list.isArray() || !delete_list.isArray())
		return callback(new_400("WRONG_JSON_TYPE"_s, "expected lists to create/delete"_s));
	if (create_list.size() + delete_list.size() <= 0)
		return callback(new_400("NO_ACTION_REQUESTED"_s, "requested update of 0 elements"_s));

	std::vector< std::size_t > add, remove, all;
	add.reserve(create_list.size());
	remove.reserve(delete_list.size());
	try
	{
		for (const Json::Value& i: create_list) add.push_back(i.asUInt64());
		for (const Json::Value& i: delete_list) remove.push_back(i.asUInt64());
	}
	catch (const Json::Exception& e)
	{
		return callback(new_400("WRONG_JSON_FORMATS", e.what()));
	}

	drogon::orm::DbClientPtr sql_client = drogon::app().getDbClient();
	drogon::orm::Mapper< Sources > mapper(sql_client);
	all.reserve(add.size() + remove.size());
	all.append_range(add);
	all.append_range(remove);
	auto res = mapper.findBy({Sources::Cols::_id, drogon::orm::CompareOperator::In, std::move(all)});
	std::vector< Sources > actual_update;
	actual_update.reserve(res.size());
	Json::Value response{Json::arrayValue};
	for (Sources& source: res)
	{
		std::size_t source_id = static_cast< std::size_t >(source.getPrimaryKey());
		std::vector< std::size_t > subs = prepare_vector(source.getSubscribers());
		std::size_t idx = std::lower_bound(subs.begin(), subs.end(), user_id) - subs.begin();
		if ((idx < subs.size()) && (subs[idx] == id))
		{
			if (std::lower_bound(remove.begin(), remove.end(), source_id) != remove.end())
			{
				subs.erase(subs.begin() + idx);
				if (subs.empty()) source.setSubscribersToNull();
				else parse_vector(source.getSubscribers(), subs);
			}
			else
			{
				response.append(source_id);
			}
		}
		else if (((idx >= subs.size()) || (subs[idx] != id))
					&& (std::lower_bound(add.begin(), add.end(), source_id) != add.end()))
		{
			if (!source.getSubscribers()) source.setSubscribers(std::vector< char >());
			subs.insert(subs.begin() + idx, id);
			parse_vector(source.getSubscribers(), subs);
			response.append(source_id);
		}
		else
		{
			continue;
		}
		mapper.update(source);
	}
	callback(new_json(std::move(response), drogon::k202Accepted));
}
