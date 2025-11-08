#pragma once

#include "Threads/ThreadData.h"
#include "Sockets/Socket.h"

class FSocketThreadData : public FThreadData
{
	friend FThreadsManager;

public:
	FSocketThreadData(FThreadsManager* InThreadsManager, const std::string& InNewThreadName);

	std::unique_ptr<FSocket> SocketPtr;
};

class FSocketManager
{
public:
	~FSocketManager();

	void CreateSockets(int32 SocketPort, bool bUseSSL, const std::string& InKeyPath = "", const std::string& InCertPath = "");

protected:
	CArray<FSocketThreadData*> SocketThreadDataArray;
};
