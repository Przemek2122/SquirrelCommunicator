#include "Logger/Logger.h"
#include "DataBase/DataBaseConnect.h"
#include "DataBase/DataBaseSettings.h"
#include "soci/session.h"
#include "soci/connection-pool.h"

#include <mutex>
#include <vector>

// Forward-declare the SOCI MySQL static backend factory
// (avoids including soci-mysql.h which drags in mysql.h client headers)
namespace soci { extern backend_factory const mysql; }

// Static members
std::unique_ptr<soci::connection_pool> FDataBaseConnect::Pool = nullptr;
bool FDataBaseConnect::bPoolInitialized = false;

// Session timeout values applied to every connection (seconds)
static constexpr const char* SESSION_VARS =
    "SET SESSION wait_timeout       = 86400, "
    "    SESSION interactive_timeout = 86400, "
    "    SESSION net_read_timeout    = 30, "
    "    SESSION net_write_timeout   = 30";

// InitPool
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

            try
            {
                Session.once << SESSION_VARS;
            }
            catch (const soci::soci_error& e)
            {
                LOG_WARN("Failed to set session timeouts on pool connection "
                    << i << " (non-fatal): " << e.what());
            }
        }

        bPoolInitialized = true;

        LOG_STATE("Database connection pool initialized with " << PoolSize << " connections.");
    }
    catch (const soci::soci_error& e)
    {
        LOG_ERROR("Failed to initialize database connection pool: " << e.what());
        Pool.reset();
    }
}

// ShutdownPool
void FDataBaseConnect::ShutdownPool()
{
    if (Pool)
    {
        Pool.reset();
        bPoolInitialized = false;

        LOG_STATE("Database connection pool shut down.");
    }
}

void FDataBaseConnect::KeepPoolAlive()
{
    static std::mutex MaintenanceMutex;

    // Use try_lock so that in case of overlapping background calls, one thread yields instead of hanging and waiting
    if (!MaintenanceMutex.try_lock())
        return;

    std::lock_guard<std::mutex> Lock(MaintenanceMutex, std::adopt_lock);

    if (!bPoolInitialized || !Pool)
        return;

    const std::string& ConnectionString = FDataBaseSettings::GetConnectionString();
    std::size_t Pos = 0;

    // RAII guard for leased connections ensuring that regardless of exceptions or crashes the destructor will always return the connections to the pool
    struct FPoolLeaseGuard {
        soci::connection_pool* PoolPtr;
        std::vector<std::size_t> LeasedSlots;

        ~FPoolLeaseGuard() {
            for (std::size_t Slot : LeasedSlots) {
                PoolPtr->give_back(Slot);
            }
        }
    } LeaseGuard{Pool.get(), {}};

    // Fetch all currently available connections without returning them immediately
    // Since we do not return them in the loop, try_lease will finish when all free connections are leased
    while (Pool->try_lease(Pos, 0))
    {
        // If push_back throws std::bad_alloc, the LeaseGuard destructor will return the already collected connections
        LeaseGuard.LeasedSlots.push_back(Pos);
    }

    size_t ReplacedCount = 0;

    // Validation with full exception safety
    for (std::size_t SlotPos : LeaseGuard.LeasedSlots)
    {
        soci::session& Session = Pool->at(SlotPos);
        bool bIsAlive = false;

        try
        {
            Session.once << "SELECT 1";
            bIsAlive = true;
        }
        catch (const soci::soci_error&)
        {
            // Expected behavior for a dead connection
        }
        catch (...)
        {
            // Catching everything saves us from a hard application crash
            LOG_ERROR("KeepPoolAlive: Unknown error during SELECT 1 on slot " << SlotPos);
        }

        if (!bIsAlive)
        {
            try
            {
                // Safe closure, ignore errors since the connection is already dead
                try { Session.close(); } catch (...) {}

                Session.open(soci::mysql, ConnectionString);
                Session.once << SESSION_VARS;

                ++ReplacedCount;
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("KeepPoolAlive: failed to replace dead connection at index "
                          << SlotPos << ": " << e.what());
            }
            catch (...)
            {
                LOG_ERROR("KeepPoolAlive: FATAL unknown exception while replacing dead connection.");
            }
        }
    }

    if (ReplacedCount > 0)
    {
        LOG_INFO("KeepPoolAlive: replaced " << ReplacedCount
                 << " dead pool connection(s) out of "
                 << LeaseGuard.LeasedSlots.size() << " free slot(s) checked");
    }

    // The LeaseGuard destructor automatically calls give_back() for all slots in the vector
}

FDataBaseConnect::FDataBaseConnect()
    : bIsConnected(false)
{
    if (bPoolInitialized && Pool)
    {
        // Pool is available, acquire session using RAII
        try
        {
            // Lease a session
            // std::unique_ptr invokes the special SOCI deleter which automatically performs give_back() upon destruction
            Session = std::make_unique<soci::session>(*Pool);

            if (ValidateConnection())
            {
                // Fast path: connection is stable
                bIsConnected = true;
            }
            else
            {
                // Dead connection
                // Repair it on the fly to prevent a connection storm
                LOG_WARN("Pooled connection dead - attempting to reconnect in place.");

                // Attempt to close the broken socket and ignore exceptions
                try { Session->close(); } catch (...) {}

                try
                {
                    const std::string& ConnectionString = FDataBaseSettings::GetConnectionString();
                    Session->open(soci::mysql, ConnectionString);
                    SetSessionTimeouts();
                    bIsConnected = true;

                    LOG_STATE("Pooled connection successfully re-established on the fly.");
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("Failed to re-establish pooled connection: " << e.what());

                    // If the server completely crashed, reset the session
                    // This triggers the internal SOCI destructor and returns the broken slot back to the pool
                    // It will be picked up by the cyclic KeepPoolAlive when the database comes back online
                    Session.reset();
                    bIsConnected = false;
                }
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Failed to lease connection from pool: " << e.what());
            bIsConnected = false;
        }
    }
    else
    {
        // No pool available, fallback for early boot or TestDataBaseConnection
        try
        {
            const std::string& ConnectionString = FDataBaseSettings::GetConnectionString();
            Session = std::make_unique<soci::session>(soci::mysql, ConnectionString);
            SetSessionTimeouts();
            bIsConnected = true;
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Standalone database connection failed: " << e.what());
            bIsConnected = false;
        }
    }
}

FDataBaseConnect::~FDataBaseConnect() = default;

bool FDataBaseConnect::ValidateConnection()
{
    if (!Session) return false;

    try
    {
        Session->once << "SELECT 1";
        return true;
    }
    catch (const soci::soci_error& e)
    {
        LOG_DEBUG("Connection validation failed: " << e.what());
        return false;
    }
}

void FDataBaseConnect::SetSessionTimeouts()
{
    if (!Session) return;

    try
    {
        Session->once << SESSION_VARS;
    }
    catch (const soci::soci_error& e)
    {
        LOG_WARN("Failed to set session timeouts (non-fatal): " << e.what());
    }
}
