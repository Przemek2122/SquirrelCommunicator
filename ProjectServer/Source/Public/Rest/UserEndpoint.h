#pragma once

#include "CrowAppEndpoint.h"

class FUserEndpoint : public FCrowAppEndpoint
{
public:
	FUserEndpoint(FProjectEngine* InProjectEngine = nullptr);

	void RegisterRoutes(crow::App<FCrowAppMiddleware>& App) override;

};
