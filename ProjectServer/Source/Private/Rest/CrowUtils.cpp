#include "Rest/CrowUtils.h"
#include "crow/json.h"

crow::response FCrowUtils::CreateResponse(const int ResponseCode, const CMap<std::string, std::string>& JsonFields)
{
	crow::json::wvalue response;

	for (const std::pair<const std::string, std::string>& JsonField : JsonFields)
	{
		response[JsonField.first] = JsonField.second;
	}

	return crow::response(ResponseCode, response);
}
