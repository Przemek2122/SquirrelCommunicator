#pragma once

#include "CrowAppEndpoint.h"

class FAuthEndpoint : public FCrowAppEndpoint
{
public:
	FAuthEndpoint(FProjectEngine* InProjectEngine = nullptr);

	void RegisterRoutes(crow::App<FCrowAppMiddleware>& App) override;

};
