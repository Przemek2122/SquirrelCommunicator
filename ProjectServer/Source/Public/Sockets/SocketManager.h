#pragma once
#include "EngineCompat.h"

#include "Sockets/Socket.h"
#include <thread>
#include <vector>
#include <memory>

/** Holds a socket and its worker thread */
struct FSocketThread
{
	std::jthread Thread;
	std::unique_ptr<FSocket> SocketPtr;
};

class FSocketManagerHelper
{
public:
	/** Broadcast some json to all given users */
	static void BroadcastDataToUsers(const FProjectEngine* ProjectEngine, const std::vector<Uint64>& ConversationUsersIds, const std::string& SerializedPayload);
};

class FSocketManager
{
public:
	~FSocketManager();

	void CreateSockets(std::string Host, int32 SocketPort, bool bUseSSL, const std::string& InKeyPath = "", const std::string& InCertPath = "");

	/** Enqueue some task for specific user */
	void EnqueueTaskForUserAtSocket(int32 InSocketId, Uint64 UserId, FFunctorLambda<void, void* /* ws */>& FunctionToCallOnSocket);

	FSocket* GetSocketById(int32 InSocketId);

protected:
	std::vector<std::unique_ptr<FSocketThread>> SocketThreads;
};
