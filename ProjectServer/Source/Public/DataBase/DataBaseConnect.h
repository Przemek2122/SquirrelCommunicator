#pragma once

#include "CoreMinimal.h"
#include "soci/session.h"

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
 * @Note sample usage above
 */
class FDataBaseConnect
{
public:
	FDataBaseConnect();
	~FDataBaseConnect();

	bool IsConnected() const { return bIsConnected; }
	soci::session& GetSession() { return Session; }

protected:
	soci::session Session;
	bool bIsConnected;
};
