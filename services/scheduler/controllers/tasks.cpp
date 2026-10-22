#include "tasks.hpp"

using api::operator""_s;

void api::scheduler::ctrl::tasks::initPathRouting()
{
	PATH_ADD("/api/scheduler/calculate", drogon::Post);
}

/**
	Method: POST
	Input:
	```json
	[
		{
			"id":int64_t
		},
		...
	]```
	Output:
	```json
	[
		{
			"id":int64_t
		},
		...
	]```
  */
void api::scheduler::ctrl::tasks::asyncHandleHttpRequest(const api::request_t& req, api::response_callback&& callback)
{
	std::uint64_t task_id = ++task_id_;
	LOG_DEBUG << "parsing #" << task_id << "...";
	api::expected< task > e = parse_task(req);
	if (!e)
	{
		LOG_DEBUG << "reject #" << task_id;
		return callback(e.error());
	}

	LOG_INFO << "calculating #" << task_id << "...";
	schedule(*e);
	LOG_INFO << "finish calculating #" << task_id;

	LOG_DEBUG << "serializing #" << task_id << "...";
	callback(serialize_result(*e));
	LOG_DEBUG << "ok #" << task_id;
}

api::expected< api::scheduler::task > api::scheduler::ctrl::tasks::parse_task(const api::request_t& req)
{
	const std::shared_ptr< Json::Value >& body = req->getJsonObject();
	if (!body)
		return api::unexpected{resp.k400_json_parse_error};
	if (!body->isArray())
		return api::unexpected{resp.k400_not_json_array};

	task res;
	res.events.reserve(body->size());
	try
	{
		for (const Json::Value& event_obj: *body)
		{
			api::expected< event > e = parse_event(event_obj);
			if (!e) return api::unexpected{e.error()};
			res.events.push_back(std::move(*e));
		}
	}
	catch (const std::exception&)
	{
		return api::unexpected{resp.k400_event_fields_format};
	}
	return res;
}
api::expected< api::scheduler::event > api::scheduler::ctrl::tasks::parse_event(const Json::Value& json)
{
	event e;
	if (!json.isMember("id"))
	{
		return api::unexpected{resp.k400_event_missing_obligatory};
	}
	e.id = json.get("id", 0).asInt64();
	return e;
}
api::response_t api::scheduler::ctrl::tasks::serialize_result(const task& tsk)
{
	std::string body;
	response_t resp = api::response::new_response(api::code::k200_ok);
	resp->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
	for (const event& e: tsk.events)
	{
		body += (body.empty() ? '[' : ',');
		body += "{\"id\":";
		body += std::to_string(e.id);
		body += '}';
	}
	body += ']';
	resp->setBody(std::move(body));
	return resp;
}
