#pragma once

#include "CrowAppEndpoint.h"

class FIntegrationEndpoint : public FCrowAppEndpoint
{
public:
	FIntegrationEndpoint(FProjectEngine* InProjectEngine = nullptr);

	void RegisterRoutes(crow::App<FCrowAppMiddleware>& App) override;

};

