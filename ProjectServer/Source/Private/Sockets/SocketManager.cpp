#include "Sockets/SocketManager.h"
#include "Threads/ThreadsManager.h"
#include "ProjectEngine.h"

FSocketThreadData::FSocketThreadData(FThreadsManager* InThreadsManager, const std::string& InNewThreadName)
	: FThreadData(InThreadsManager, InNewThreadName)
{
}

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
	FThreadsManager* ThreadsManager = FGlobalDefines::GEngine->GetThreadsManager();
	for (FSocketThreadData* ThreadDataArray : SocketThreadDataArray)
	{
		ThreadsManager->TryStopThread(ThreadDataArray);
	}
}

void FSocketManager::CreateSockets(std::string Host, int32 SocketPort, bool bUseSSL, const std::string& InKeyPath, const std::string& InCertPath)
{
	static const std::string SocketThreadName = "SocketThread";
	const int32 NumberOFSocketsToCreate = FThreadsManager::GetNumberOfLogicalCPU();

	SocketThreadDataArray.SetNum(NumberOFSocketsToCreate);

	FThreadsManager* ThreadsManager = FGlobalDefines::GEngine->GetThreadsManager();

	for (int32 i = 0; i < NumberOFSocketsToCreate; ++i)
	{
		const std::string CurrentSocketThreadName = SocketThreadName + std::to_string(i);

		FSocketThreadData* SocketManagerThreadData = ThreadsManager->CreateThread<FGenericThread, FSocketThreadData>(CurrentSocketThreadName);
		SocketThreadDataArray[i] = SocketManagerThreadData;

		FGenericThread* GenericThread = dynamic_cast<FGenericThread*>(SocketManagerThreadData->GetThread());
		if (GenericThread != nullptr)
		{
			GenericThread->AddTask([this, SocketManagerThreadData, i, Host, SocketPort, bUseSSL, InKeyPath, InCertPath]()
			{
				SocketManagerThreadData->SocketPtr = std::make_unique<FSocket>(i, Host, SocketPort, bUseSSL, InKeyPath, InCertPath);
				if (SocketManagerThreadData->SocketPtr != nullptr)
				{
					SocketManagerThreadData->SocketPtr->Async();
				}
			});

			GenericThread->BeginAsyncWork();
		}
	}
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
	if (SocketThreadDataArray.IsValidIndex(InSocketId))
	{
		return SocketThreadDataArray[InSocketId]->SocketPtr.get();
	}

	return nullptr;
}
