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

    if (!MaintenanceMutex.try_lock())
        return;

    std::lock_guard<std::mutex> Lock(MaintenanceMutex, std::adopt_lock);

    if (!bPoolInitialized || !Pool)
        return;

    const std::string& ConnectionString = FDataBaseSettings::GetConnectionString();

    // Custom RAII wrapper to guarantee give_back() is called even on std::bad_alloc
    struct FScopedLease {
        soci::connection_pool* PoolPtr;
        std::size_t Pos;
        bool bIsLeased;

        FScopedLease(soci::connection_pool* InPool) : PoolPtr(InPool), Pos(0), bIsLeased(false) {}
        ~FScopedLease() { if (bIsLeased) PoolPtr->give_back(Pos); }

        bool TryLease() {
            bIsLeased = PoolPtr->try_lease(Pos, 0);
            return bIsLeased;
        }
    };

    std::vector<std::size_t> FreeSlots;
    {
        FScopedLease Lease(Pool.get());
        while (Lease.TryLease())
        {
            // If push_back throws std::bad_alloc, FScopedLease destructor returns the slot
            FreeSlots.push_back(Lease.Pos);
            Lease.PoolPtr->give_back(Lease.Pos);
            Lease.bIsLeased = false;
        }
    }

    size_t ReplacedCount = 0;

    for (std::size_t SlotPos : FreeSlots)
    {
        FScopedLease Lease(Pool.get());
        Lease.Pos = SlotPos;
        Lease.bIsLeased = Pool->try_lease(Lease.Pos, 0);

        if (!Lease.bIsLeased)
            continue;

        try
        {
            soci::session& Session = Pool->at(Lease.Pos);
            Session.once << "SELECT 1";
        }
        catch (const soci::soci_error&)
        {
            try
            {
                soci::session& Session = Pool->at(Lease.Pos);

                try { Session.close(); }
                catch (...) { /* expected for dead connections */ }

                Session.open(soci::mysql, ConnectionString);
                Session.once << SESSION_VARS;

                ++ReplacedCount;
            }
            catch (const std::exception& e) // Catch standard exceptions too
            {
                LOG_ERROR("KeepPoolAlive: failed to replace dead connection at index "
                    << Lease.Pos << ": " << e.what());
            }
            catch (...)
            {
                LOG_ERROR("KeepPoolAlive: unknown exception while replacing dead connection.");
            }
        }

        // FScopedLease destructor automatically calls give_back() here
    }

    if (ReplacedCount > 0)
    {
        LOG_INFO("KeepPoolAlive: replaced " << ReplacedCount
            << " dead pool connection(s) out of "
            << FreeSlots.size() << " free slot(s) checked");
    }
}

FDataBaseConnect::FDataBaseConnect()
    : bIsConnected(false)
{
    if (bPoolInitialized && Pool)
    {
        // pool available
        try
        {
            // Lease a session (blocks until one is free).
            Session = std::make_unique<soci::session>(*Pool);

            if (ValidateConnection())
            {
                // Fast path: pooled connection is alive.
                bIsConnected = true;
            }
            else
            {
                // Connection is dead.  Destroy the leased session
                // (returns the dead slot to the pool as "free")
                // and create a standalone connection instead.
                LOG_WARN("Pooled connection dead — using standalone fallback"
                    " (slot will be revived by KeepPoolAlive).");

                Session.reset();  // returns dead slot to pool

                const std::string& ConnectionString =
                    FDataBaseSettings::GetConnectionString();
                Session = std::make_unique<soci::session>(
                    soci::mysql, ConnectionString);
                SetSessionTimeouts();
                bIsConnected = true;
            }
        }
        catch (const soci::soci_error& e)
        {
            LOG_ERROR("Failed to lease connection from pool: " << e.what());
            bIsConnected = false;
        }
    }
    else
    {
        // no pool (early boot / TestDataBaseConnection)
        const std::string& ConnectionString =
            FDataBaseSettings::GetConnectionString();

        try
        {
            Session = std::make_unique<soci::session>(
                soci::mysql, ConnectionString);
            SetSessionTimeouts();
            bIsConnected = true;
        }
        catch (const soci::soci_error& e)
        {
            LOG_ERROR("Database connection failed: " << e.what());
            bIsConnected = false;
        }
    }
}

FDataBaseConnect::~FDataBaseConnect()
{
    // soci::session destructor automatically:
    //   - returns the connection to the pool (if leased), or
    //   - closes the standalone connection
}

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
