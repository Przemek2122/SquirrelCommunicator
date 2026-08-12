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

        LOG_INFO("Database connection pool initialized with " << PoolSize << " connections.");
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

        LOG_INFO("Database connection pool shut down.");
    }
}

// KeepPoolAlive — periodic pool maintenance (runs from main tick)
//
// Strategy: use try_lease to discover and test ONLY unleased (free) slots.
// Slots currently leased to other threads are never touched, avoiding
// concurrent access to the same soci::session.
void FDataBaseConnect::KeepPoolAlive()
{
    static std::mutex MaintenanceMutex;

    // Non-blocking: if another thread is already running maintenance, skip.
    if (!MaintenanceMutex.try_lock())
        return;

    std::lock_guard<std::mutex> Lock(MaintenanceMutex, std::adopt_lock);

    if (!bPoolInitialized || !Pool)
        return;

    const std::string& ConnectionString = FDataBaseSettings::GetConnectionString();

    // collect all currently-free slot positions ---
    std::vector<std::size_t> FreeSlots;
    {
        std::size_t Pos = 0;
        while (Pool->try_lease(Pos, 0))   // 0 = don't wait for a busy slot
        {
            FreeSlots.push_back(Pos);
            Pool->give_back(Pos);
        }
    }

    // validate each free slot, revive dead ones ---
    size_t ReplacedCount = 0;

    for (std::size_t SlotPos : FreeSlots)
    {
        // Re-lease.  If another thread grabbed it since pass 1, skip.
        std::size_t Pos = SlotPos;
        if (!Pool->try_lease(Pos, 0))
            continue;

        try
        {
            soci::session& Session = Pool->at(Pos);
            Session.once << "SELECT 1";
        }
        catch (const soci::soci_error&)
        {
            // Connection is dead — close and reopen in-place.
            try
            {
                soci::session& Session = Pool->at(Pos);

                // close() may throw if the socket is in a bad state.
                try { Session.close(); }
                catch (...) { /* expected for dead connections */ }

                Session.open(soci::mysql, ConnectionString);
                Session.once << SESSION_VARS;

                ++ReplacedCount;
            }
            catch (const soci::soci_error& e2)
            {
                LOG_ERROR("KeepPoolAlive: failed to replace dead connection"
                    " at index " << Pos << ": " << e2.what());
            }
        }

        Pool->give_back(Pos);
    }

    if (ReplacedCount > 0)
    {
        LOG_INFO("KeepPoolAlive: replaced " << ReplacedCount
            << " dead pool connection(s) out of "
            << FreeSlots.size() << " free slot(s) checked");
    }
}

// ---------------------------------------------------------------------------
// Constructor
//
// Flow:
//   1. Lease a session from the pool (blocks until one is free).
//   2. Validate with SELECT 1.
//   3. If alive  → use it (optimal path, ~1 µs overhead for the ping).
//   4. If dead   → return it to the pool (slot freed, connection dead)
//                  and create a fresh standalone connection.
//                  KeepPoolAlive() will revive the dead slot later.
//   5. No pool  → standalone connection directly.
//
// IMPORTANT: we NEVER call close()/open() on a leased session because
// that would replace the lease wrapper's backend with a regular backend,
// breaking the pool's internal tracking.  Pool repair is done exclusively
// by KeepPoolAlive() through properly-leased slots.
// ---------------------------------------------------------------------------
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
