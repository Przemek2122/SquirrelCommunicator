// Created by https://www.linkedin.com/in/przemek2122/ 2020-2026

#pragma once

#include "EngineCompat.h"

/** Represents single-server instance */
class FServer
{
public:
    FServer();

    std::string GetServerName() const { return ServerName; }

private:
    /** Server id */
    Uint64 ServerId;

    /** Displayed  server name */
    std::string ServerName;

    /** Client ids of this server */
    std::vector<Uint64> Clients;

};
