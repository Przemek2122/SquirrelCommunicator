#include "Sockets/SocketManager.h"
#include "ThreadCompat.h"
#include "ProjectEngine.h"

void FSocketManagerHelper::BroadcastDataToUsers(const FProjectEngine* ProjectEngine, const std::vector<Uint64>& ConversationUsersIds, const std::string& SerializedPayload)
{
	FUserManager* UserManager = ProjectEngine->GetUserManager();
	FSocketManager* SocketManager = ProjectEngine->GetSocketManager();

	std::vector<std::shared_ptr<FUser>> OutIds;
	UserManager->GetUsersByIds(ConversationUsersIds, OutIds);

	for (std::shared_ptr<FUser>& UserPtr : OutIds)
	{
		if (UserPtr != nullptr)
		{
			const FUser* User = UserPtr.get();
			const Uint64 UserId = User->GetUserId();

			FFunctorLambda<void, void*> SocketAccessFunctor = [SerializedPayload, UserId](void* targetWs)
			{
				auto* WebSocket = static_cast<uWS::WebSocket<false, true, FWebSocketSessionData>*>(targetWs);

				// @TODO: Do we actually need to check if the user is subscribed to the topic?
				const std::string UserTopic = FSocket::GenerateUserTopic(UserId);
				if (WebSocket->isSubscribed(UserTopic))
				{
					WebSocket->send(SerializedPayload, uWS::OpCode::TEXT);
				}
				else
				{
					LOG_DEBUG("Not supported topic means something is wrong");
				}
			};

			SocketManager->EnqueueTaskForUserAtSocket(User->GetSocketId(), UserId, SocketAccessFunctor);
		}
	}
}

FSocketManager::~FSocketManager()
{
	// std::jthread auto-requests stop and joins on destruction
	SocketThreads.clear();
}

void FSocketManager::CreateSockets(std::string Host, int32 SocketPort, bool bUseSSL, const std::string& InKeyPath, const std::string& InCertPath)
{
	const int32 NumberOfSocketsToCreate = GetNumberOfLogicalCPU();

	SocketThreads.reserve(NumberOfSocketsToCreate);

	LOG_INFO("Creating threads for Sockets (" << NumberOfSocketsToCreate << ") listening host... '" << Host << ".");

	for (int32 i = 0; i < NumberOfSocketsToCreate; ++i)
	{
		auto SocketThread = std::make_unique<FSocketThread>();

		// Capture index and params by value for the thread
		const int32 SocketIndex = i;
		SocketThread->Thread = std::jthread([SocketThread = SocketThread.get(), SocketIndex, Host, SocketPort, bUseSSL, InKeyPath, InCertPath](const std::stop_token&)
		{
			SocketThread->SocketPtr = std::make_unique<FSocket>(SocketIndex, Host, SocketPort, bUseSSL, InKeyPath, InCertPath);
			if (SocketThread->SocketPtr != nullptr)
			{
				// FSocket::Async() calls uWS::run() which blocks until the loop ends
				SocketThread->SocketPtr->Async();
			}
		});

		SocketThreads.push_back(std::move(SocketThread));
	}

	LOG_INFO("Socket threads queued: " << SocketThreads.size() << "/" << NumberOfSocketsToCreate << ".");
}

void FSocketManager::EnqueueTaskForUserAtSocket(const int32 InSocketId, const Uint64 UserId, FFunctorLambda<void, void*>& FunctionToCallOnSocket)
{
	FSocket* Socket = GetSocketById(InSocketId);
	if (Socket != nullptr)
	{
		Socket->AddDeferTaskForConnectionId(UserId, FunctionToCallOnSocket);
	}
}

FSocket* FSocketManager::GetSocketById(int32 InSocketId)
{
	if (InSocketId >= 0 && InSocketId < static_cast<int32>(SocketThreads.size()))
	{
		return SocketThreads[InSocketId]->SocketPtr.get();
	}

	return nullptr;
}
