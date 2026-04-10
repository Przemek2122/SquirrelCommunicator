// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

#include "CrowAppEndpoint.h"

/** Endpoint for managing rooms service (GO) endpoint */
class FRoomsServiceEndpoint : public FCrowAppEndpoint
{
public:
	FRoomsServiceEndpoint(FProjectEngine* InProjectEngine = nullptr);

	void RegisterRoutes(crow::App<FCrowAppMiddleware>& App) override;
	
};
