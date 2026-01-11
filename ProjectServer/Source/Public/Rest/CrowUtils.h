#pragma once

#include "crow/http_response.h"

#define CROW_RESPONSE(CODE, STATUS, MESSAGE) FCrowUtils::CreateResponse(crow::status::CODE, \
	{ \
		{ FPredefinedMessages::Status::Name, FPredefinedMessages::Status::STATUS }, \
		MESSAGE \
	} \
);

class FCrowUtils
{
public:
	static crow::response CreateResponse(const int ResponseCode, const CMap<std::string, std::string>& JsonFields);
};