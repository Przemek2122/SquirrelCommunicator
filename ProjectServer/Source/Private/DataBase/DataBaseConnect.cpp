#include "Logger/Logger.h"
#include "DataBase/DataBaseConnect.h"
#include "DataBase/DataBaseSettings.h"
#include "soci/session.h"

FDataBaseConnect::FDataBaseConnect()
{
	const std::string& ConnectionString = FDataBaseSettings::GetConnectionString();
    static const std::string BackendName = "mysql";

    try
    {
        // Create connection
		Session = std::make_unique<soci::session>(BackendName, ConnectionString);
        bIsConnected = true;
    }
    catch (const soci::soci_error& e)
    {
        LOG_ERROR("Database error: " << e.what());
        bIsConnected = false;
    }
}

FDataBaseConnect::~FDataBaseConnect()
{
}
