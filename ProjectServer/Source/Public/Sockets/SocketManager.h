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

	void EnqueueTaskForUserAtSocket(int32 InSocketId, Uint64 UserId, FFunctorLambda<void, void* /* ws */>& FunctionToCallOnSocket);

	FSocket* GetSocketById(int32 InSocketId);

protected:
	CArray<FSocketThreadData*> SocketThreadDataArray;
};
