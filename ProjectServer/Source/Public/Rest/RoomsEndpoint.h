// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

#include "CrowAppEndpoint.h"

class FRoomsEndpoint : public FCrowAppEndpoint
{
public:
	FRoomsEndpoint(FProjectEngine* InProjectEngine = nullptr);

	void RegisterRoutes(crow::App<FCrowAppMiddleware>& App) override;
	
};
