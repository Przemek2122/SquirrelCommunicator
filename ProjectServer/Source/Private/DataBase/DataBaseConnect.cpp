#include "Logger/Logger.h"
#include "DataBase/DataBaseConnect.h"
#include "DataBase/DataBaseSettings.h"
#include "soci/session.h"

// Forward-declare the SOCI MySQL static backend factory
// (avoids including soci-mysql.h which drags in mysql.h client headers)
namespace soci { extern backend_factory const mysql; }

FDataBaseConnect::FDataBaseConnect()
{
	const std::string& ConnectionString = FDataBaseSettings::GetConnectionString();

    try
    {
        // Use static factory directly (avoids dlopen for libsoci_mysql.so)
		Session = std::make_unique<soci::session>(soci::mysql, ConnectionString);
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
