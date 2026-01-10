#pragma once

#include "CrowAppMiddleware.h"
#include "crow/app.h"

class FProjectEngine;

/** Class for endpoints using crow */
class FCrowAppEndpoint
{
public:
	FCrowAppEndpoint(FProjectEngine* InProjectEngine = nullptr);
	virtual ~FCrowAppEndpoint() { }

	virtual void RegisterRoutes(crow::App<FCrowAppMiddleware>& App);

protected:
	FProjectEngine* ProjectEngine;
	
};
