#include "tasks.hpp"

api::scheduler::ctrl::tasks::responses::responses()
{
	k400_json_parse_error->setBody("{\"error\":{\"code\":\"JSON_PARSE_ERROR\",\"msg\":\"wrong json format\"}}");
	k400_not_json_array->setBody("{\"error\":{\"code\":\"NOT_JSON_ARRAY\",\"msg\":\"expected json array\"}}");
	k400_event_missing_obligatory->setBody("{\"error\":{\"code\":\"EVENT_MISSING_OBLIGATORY\","
				"\"msg\":\"missing obligatory fields in event description\"}}");
	k400_event_fields_format->setBody("{\"error\":{\"code\":\"EVENT_FORMAT\","
				"\"msg\":\"wrong event fields format\"}}");
}
