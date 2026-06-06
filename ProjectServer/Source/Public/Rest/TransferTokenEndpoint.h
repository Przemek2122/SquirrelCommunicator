// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once
#include "CrowAppEndpoint.h"

class FTransferTokenEndpoint : public FCrowAppEndpoint
{
public:
    FTransferTokenEndpoint(FProjectEngine* InProjectEngine = nullptr);

    void RegisterRoutes(crow::App<FCrowAppMiddleware>& App) override;

};
