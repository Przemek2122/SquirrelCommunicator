#pragma once

#include "crow/http_response.h"

class FCrowUtils
{
public:
	static crow::response CreateResponse(const int ResponseCode, const CMap<std::string, std::string>& JsonFields);
};