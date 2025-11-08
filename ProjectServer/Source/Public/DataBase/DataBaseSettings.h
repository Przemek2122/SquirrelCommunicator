#pragma once

#include "CoreMinimal.h"

class FProjectEngine;

/** Struct with DB settings for connection */
struct FDataBaseConnectionData
{
    FDataBaseConnectionData();

    std::string Host;
    std::string Port;
    std::string DataBaseName;
    std::string UserName;
    std::string Password;
};

/** Global cached database settings */
class FDataBaseSettings
{
    friend FProjectEngine;

public:
    static const FDataBaseConnectionData& GetDataBaseConnectionData();
    static const std::string& GetConnectionString();

private:
    static void Initialize();

    static std::string GetEnvHost();
    static std::string GetEnvPort();
    static std::string GetEnvDataBaseName();
    static std::string GetEnvUser();
    static std::string GetEnvPassword();
};
