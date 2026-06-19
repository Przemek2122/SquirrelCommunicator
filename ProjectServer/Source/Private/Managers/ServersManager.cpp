// Created by https://www.linkedin.com/in/przemek2122/ 2020-2026

#include "Managers/ServersManager.h"

FServersManager::FServersManager()
{
}

std::shared_ptr<FServer> FServersManager::GetServerById(const Uint64 InServerId)
{
    std::shared_lock Lock(ServersMapMutex);

    // Find server by iterator
    auto ServerIter = ServersMap.find(InServerId);
    if (ServerIter != ServersMap.end())
    {
        return ServerIter->second;
    }

    return nullptr;
}

Uint64 FServersManager::AddServer(const std::string& InServerName)
{



    return 0;
}

bool FServersManager::RemoveServer(Uint64 InServerId)
{


    return false;
}

bool FServersManager::UploadNewServerToDB(std::shared_ptr<FServer> ServerPtr)
{


    return false;
}

bool FServersManager::DeleteServerFromDB(Uint64 InServerId)
{


    return false;
}
