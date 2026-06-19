// Created by https://www.linkedin.com/in/przemek2122/ 2020-2026

#pragma once

#include "EngineCompat.h"

#include <shared_mutex>

class FServer;

/**
 * Manager for user servers
 * Currently should only handle server instances
 *
 * In future should handle:
 *  - server creation and deletion
 *  - server invites
 */
class FServersManager
{
public:
    FServersManager();

    /** Search for server with provided Id, will return nullptr if not found */
    std::shared_ptr<FServer> GetServerById(Uint64 InServerId);

    /** Add server and returns Id of new server */
    Uint64 AddServer(const std::string& InServerName);

    /** Remove server with provided Id */
    bool RemoveServer(Uint64 InServerId);

protected:
    bool UploadNewServerToDB(std::shared_ptr<FServer> ServerPtr);
    bool DeleteServerFromDB(Uint64 InServerId);

private:
    /** Server Id to server instance map */
    std::unordered_map<Uint64, std::shared_ptr<FServer>> ServersMap;

    /** Mutex for servers map mutex */
    std::shared_mutex ServersMapMutex;

};
