#include "Managers/ConversationsManager.h"

void FConversationsManager::AddConversation(const Uint64 InConversationId, const CArray<Uint64>& InUserIds)
{
	if (!ConversationIdToConversationData.ContainsKey(InConversationId))
	{
		std::shared_ptr<FConversationData> ConversationDataSharedPtr = std::make_shared<FConversationData>();
		ConversationDataSharedPtr->ConversationId = InConversationId;
		ConversationDataSharedPtr->UsersIds = InUserIds;

		ConversationIdToConversationData.InsertOrAssign(InConversationId, ConversationDataSharedPtr);
	}
}

std::shared_ptr<FConversationData> FConversationsManager::GetConversation(const Uint64 InConversationId)
{
	std::shared_ptr<FConversationData> ConversationDataSharedPtr;

	if (ConversationIdToConversationData.ContainsKey(InConversationId))
	{
		ConversationDataSharedPtr = ConversationIdToConversationData[InConversationId];
	}

	return ConversationDataSharedPtr;
}
