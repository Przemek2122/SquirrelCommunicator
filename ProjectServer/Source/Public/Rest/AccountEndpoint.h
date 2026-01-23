// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

#include "CrowAppEndpoint.h"

class FAccountEndpoint : public FCrowAppEndpoint
{
public:
    FAccountEndpoint(FProjectEngine* InProjectEngine = nullptr);

    void RegisterRoutes(crow::App<FCrowAppMiddleware>& App) override;
};