#include "profiles.hpp"

api::users::ctrl::profiles::responses::responses()
{
	k400_not_json->setBody("{\"error\":{\"code\":\"NOT_JSON\",\"msg\":\"expected json object\"}}");
	k400_missing_obligatory_login->setBody("{\"error\":{\"code\":\"MISSING_OBLIGATORY\","
				"\"msg\":\"missing login or password in json\"}}");
	k400_missing_obligatory_register->setBody("{\"error\":{\"code\":\"MISSING_OBLIGATORY\","
				"\"msg\":\"missing login, email or password in json\"}}");
	k400_failed_auth->setBody("{\"error\":{\"code\":\"FAILED\",\"msg\":\"wrong login or password\"}}");
	k400_nickname_busy->setBody("{\"error\":{\"code\":\"NICKNAME_BUSY\",\"msg\":\"nickname is busy\"}}");
	k400_email_busy->setBody("{\"error\":{\"code\":\"EMAIL_BUSY\",\"msg\":\"this email is already in use\"}}");
	k400_password_too_short->setBody(std::format("{{\"error\":{{\"code\":\"PW_TOO_SHORT\","
				"\"msg\":\"password must be {}+ symbols\"}}}}", pw_min_length));
	k400_unsupported_password->setBody("{\"error\":{\"code\":\"PW_UNSUPPORTED\","
				"\"msg\":\"unsupported password - try to change it somehow\"}}");

	k500_db_error->setBody("{\"error\":{\"code\":\"DB_ERROR\","
				"\"msg\":\"internal database error\"}}");
}
