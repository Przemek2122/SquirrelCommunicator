#include "Sockets/SocketManager.h"
#include "Threads/ThreadsManager.h"

FSocketThreadData::FSocketThreadData(FThreadsManager* InThreadsManager, const std::string& InNewThreadName)
	: FThreadData(InThreadsManager, InNewThreadName)
{
}

FSocketManager::~FSocketManager()
{
	FThreadsManager* ThreadsManager = FGlobalDefines::GEngine->GetThreadsManager();
	for (FSocketThreadData* ThreadDataArray : SocketThreadDataArray)
	{
		ThreadsManager->TryStopThread(ThreadDataArray);
	}
}

void FSocketManager::CreateSockets(int32 SocketPort, bool bUseSSL, const std::string& InKeyPath, const std::string& InCertPath)
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
			GenericThread->AddTask([this, SocketManagerThreadData, i, SocketPort, bUseSSL, InKeyPath, InCertPath]()
			{
				SocketManagerThreadData->SocketPtr = std::make_unique<FSocket>(i, SocketPort, bUseSSL, InKeyPath, InCertPath);
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
