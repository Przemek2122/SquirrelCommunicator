#pragma once

#include "CoreMinimal.h"

struct FConversationMessageData
{
	Uint64 MessageId;

	/** Who sent message? */
	Uint64 SenderId;

	/** Actual message */
	std::string Message;

};

/** Struct for each conversation */
struct FConversationData
{
	Uint64 ConversationId;

	/** Users */
	CArray<Uint64> UsersIds;

	/** Messages map. */
	CDeque<FConversationMessageData, Uint64> MessagesMap;

};

/**
 * Class for managing user conversations
 * Stores users of conversations to sent quickly any message received
 * Conversations are added into ConversationIdToConversationData when any user creates one or sent messages
 */
class FConversationsManager
{
public:
	void AddConversation(Uint64 InConversationId, const CArray<Uint64>& InUserIds);
	std::shared_ptr<FConversationData> GetConversation(Uint64 InConversationId);

protected:
	/** Map with conversations mapped into their data structs */
	CUnorderedMap<Uint64, std::shared_ptr<FConversationData>, Uint64> ConversationIdToConversationData;

};
