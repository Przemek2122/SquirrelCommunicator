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

/*
	FDataBaseConnect Connect;
	if (Connect.IsConnected())
	{
		try
		{
			// Get database connection session
			soci::session& DataBaseSession = Connect.GetSession();
			soci::indicator Ind;

			DataBaseSession << "INSERT INTO messages (conversation_id, sender_id, text) VALUES (:conv_id, :sender_id, :text)",
				soci::use(InConversationId),
				soci::use(SenderId),
				soci::use(InMessage, Ind);

			if (Ind == soci::i_ok)
			{
				
			}
		}
		catch (const soci::soci_error& Error)
		{
			// Handle SOCI error
			LOG_ERROR("Database error: " << Error.what());
		}
	}
 */

/**
 * Class for MYSQL connections
 * https://soci.sourceforge.net/doc/master/backends/mysql/
 *
 * Uses a connection pool internally so that each FDataBaseConnect instance
 * borrows a pre-opened session instead of creating a new TCP connection.
 * The session is automatically returned to the pool when this object is destroyed.
 *
 * @Note sample usage above
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
	 * Must be called once after FDataBaseSettings::Initialize() and before
	 * any FDataBaseConnect instances are created.
	 * @param PoolSize Number of connections to keep in the pool (default 10).
	 */
	static void InitPool(size_t PoolSize = 10);

	/** Shutdown the connection pool and close all connections. */
	static void ShutdownPool();

protected:
	/** Session borrowed from the pool (returned on destruction) */
	std::unique_ptr<soci::session> Session;
	bool bIsConnected;

private:
	/** Global connection pool (created by InitPool, destroyed by ShutdownPool) */
	static std::unique_ptr<soci::connection_pool> Pool;
	static bool bPoolInitialized;
};
