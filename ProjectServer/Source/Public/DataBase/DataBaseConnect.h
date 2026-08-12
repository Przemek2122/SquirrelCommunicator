#pragma once

#include "EngineCompat.h"
#include "soci/session.h"

namespace soci { class connection_pool; }

/** Enum for returning DB operation result */
enum class EDatabaseOperationResult : Uint8
{
	Unknown,
	Success,
	OperationFailed,
	ConnectionFailed,
	DatabaseFailed,
	DataNotFound
};


/**
 * RAII database connection handle with automatic health-checking.
 *
 * **At borrow time** (constructor):
 *  1. Lease a session from the global connection pool.
 *  2. Validate with a cheap "SELECT 1" ping.
 *  3. If alive → use it.
 *  4. If dead  → return it to the pool as-is (slot freed, connection dead),
 *     then create a fresh standalone connection for this instance.
 *     The dead slot will be revived by KeepPoolAlive() during the next
 *     maintenance cycle.
 *
 * When the pool has not been initialized yet (early boot), a standalone
 * connection is created directly — no pool involved.
 *
 * **Pool maintenance** (KeepPoolAlive):
 *  Runs periodically (e.g. every 30 min from the main tick).  Uses
 *  try_lease() to discover free slots, validates each with SELECT 1,
 *  and reconnects dead ones by closing and reopening them.  Slots
 *  currently leased to other threads are never touched — only free
 *  (idle) connections are maintained.  This avoids concurrent access
 *  to the same soci::session from two threads.
 *
 * Usage pattern (unchanged — zero caller changes):
 * @code
 *   FDataBaseConnect Connect;
 *   if (Connect.IsConnected())
 *   {
 *       soci::session& Session = Connect.GetSession();
 *       // … run queries …
 *   }
 * @endcode
 *
 * Every session carries these MySQL session variables:
 *   - wait_timeout        = 86400   (24 h)
 *   - interactive_timeout = 86400
 *   - net_read_timeout    = 30      (prevents query hangs)
 *   - net_write_timeout   = 30
 */
class FDataBaseConnect
{
public:
	FDataBaseConnect();
	~FDataBaseConnect();

	bool IsConnected() const { return bIsConnected; }
	soci::session& GetSession() { return *Session; }

	/**
	 * Initialize the global connection pool.
	 * Must be called once after FDataBaseSettings::Initialize() and
	 * before any FDataBaseConnect instances are created.
	 * @param PoolSize Number of connections to keep (default 10).
	 */
	static void InitPool(size_t PoolSize = 10);

	/** Shut down the global connection pool and close all connections. */
	static void ShutdownPool();

	/**
	 * Walk every *free* slot in the pool, ping with SELECT 1, and
	 * reconnect dead connections in-place.  Slots currently leased to
	 * other threads are skipped (they are still in use and assumed alive).
	 *
	 * Should be called periodically from the main tick (e.g. every 30 min).
	 *
	 * This is the *only* place where pool-internal sessions are repaired.
	 * The borrow-time path (constructor) never mutates a pooled session
	 * — it only validates and falls back to a standalone connection.
	 */
	static void KeepPoolAlive();

protected:
	/** Session: either leased from pool or standalone. */
	std::unique_ptr<soci::session> Session;
	bool bIsConnected;

private:
	/**
	 * Ping the current session with "SELECT 1".
	 * @return true if alive, false if the connection is dead.
	 */
	bool ValidateConnection();

	/**
	 * Apply MySQL session variables (timeouts) on the current session.
	 * Safe to call multiple times; failures are non-fatal.
	 */
	void SetSessionTimeouts();

	/** Global connection pool (created by InitPool, destroyed by ShutdownPool). */
	static std::unique_ptr<soci::connection_pool> Pool;
	static bool bPoolInitialized;
};
