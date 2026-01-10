#pragma once

#include "CrowAppEndpoint.h"

class FTestEndpoint : public FCrowAppEndpoint
{
public:
	FTestEndpoint(FProjectEngine* InProjectEngine = nullptr);

	void RegisterRoutes(crow::App<FCrowAppMiddleware>& App) override;

};

