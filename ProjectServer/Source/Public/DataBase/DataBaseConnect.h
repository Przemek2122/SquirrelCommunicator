#pragma once

#include "CoreMinimal.h"
#include "soci/session.h"

/**
 * Class for MYSQL connections
 * https://soci.sourceforge.net/doc/master/backends/mysql/
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
