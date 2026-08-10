#include "Logger/Logger.h"
#include "DataBase/DataBaseConnect.h"
#include "DataBase/DataBaseSettings.h"
#include "soci/session.h"
#include "soci/connection-pool.h"

// Forward-declare the SOCI MySQL static backend factory
// (avoids including soci-mysql.h which drags in mysql.h client headers)
namespace soci { extern backend_factory const mysql; }

// Static members
std::unique_ptr<soci::connection_pool> FDataBaseConnect::Pool = nullptr;
bool FDataBaseConnect::bPoolInitialized = false;

void FDataBaseConnect::InitPool(const size_t PoolSize)
{
    if (bPoolInitialized)
    {
        LOG_WARN("FDataBaseConnect::InitPool called more than once, ignoring.");
        return;
    }

    const std::string& ConnectionString = FDataBaseSettings::GetConnectionString();

    Pool = std::make_unique<soci::connection_pool>(PoolSize);

    try
    {
        for (size_t i = 0; i < PoolSize; ++i)
        {
            soci::session& Session = Pool->at(i);
            Session.open(soci::mysql, ConnectionString);
        }

        bPoolInitialized = true;

        LOG_INFO("Database connection pool initialized with " << PoolSize << " connections.");
    }
    catch (const soci::soci_error& e)
    {
        LOG_ERROR("Failed to initialize database connection pool: " << e.what());
        Pool.reset();
    }
}

void FDataBaseConnect::ShutdownPool()
{
    if (Pool)
    {
        Pool.reset();
        bPoolInitialized = false;

        LOG_INFO("Database connection pool shut down.");
    }
}

FDataBaseConnect::FDataBaseConnect()
    : bIsConnected(false)
{
    if (bPoolInitialized && Pool)
    {
        try
        {
            // Borrow a session from the pool.
            // The soci::session constructor with a connection_pool& will block
            // until a connection becomes available, then lease it.
            // When the session is destroyed, it is automatically returned to the pool.
            Session = std::make_unique<soci::session>(*Pool);
            bIsConnected = true;
        }
        catch (const soci::soci_error& e)
        {
            LOG_ERROR("Failed to acquire session from pool: " << e.what());
            bIsConnected = false;
        }
    }
    else
    {
        // Fallback: create a standalone connection (pool not initialized yet).
        // This keeps backward compatibility for code that runs before InitPool()
        // (e.g. TestDataBaseConnection during engine init).
        const std::string& ConnectionString = FDataBaseSettings::GetConnectionString();

        try
        {
            Session = std::make_unique<soci::session>(soci::mysql, ConnectionString);
            bIsConnected = true;
        }
        catch (const soci::soci_error& e)
        {
            LOG_ERROR("Database error: " << e.what());
            bIsConnected = false;
        }
    }
}

FDataBaseConnect::~FDataBaseConnect()
{
    // soci::session destructor automatically returns the connection to the pool
    // if it was acquired from one, or closes it if standalone.
}
