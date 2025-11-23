#pragma once

#include "CoreMinimal.h"
#include <shared_mutex>

namespace soci
{
	class session;
}

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

/** Struct for user cached conversations */
struct FUserConversations
{
	CArray<Uint64> Conversations;
};

/** Temporary structure for db DownloadConversationParticipants */
struct FConversationInfo
{
	long long ConversationId;
	std::string LastMessageAt;
	long long LastReadMessageId;
};

/**
 * Class for managing user conversations
 * Stores users of conversations to sent quickly any message received
 * Conversations are added into ConversationIdToConversationData when any user creates one or sent messages
 * ALL downloading and managing conversations should be done using this class
 */
class FConversationsManager
{
public:
	/** Create new conversation */
	Uint64 GetOrCreateConversation(const CArray<Uint64>& InUserIds);

	std::shared_ptr<FConversationData> GetConversation(Uint64 InConversationId);
	bool HasConversation(Uint64 InConversationId);

	void GetLastConversationByUserId(Uint64 InUserId, int32 Offset, int32 Limit, CArray<Uint64>& OutConversationIds);

private:
	/** Query DB for conversations of user */
	void DownloadConversationsFromRange(Uint64 UserId, int32 Offset, int32 Limit);

	/** Query DB for participants */
	void DownloadConversationParticipants(Uint64 InConversationId, CArray<Uint64>& OutUserIds);

	/**
	 * Conditional add conversation to DB
	 * If exists, we will not add another conversation, we will return previous one matching
	 */
	Uint64 UploadOrGetConversation(const std::vector<Uint64>& UserIds);

	/** UploadConversation helper for conversation between two people */
	Uint64 FindConversation2Ids(soci::session& Sql, Uint64 User1Id, Uint64 User2Id);

	/**  */
	Uint64 FindConversationNIds(soci::session& Sql, const std::vector<Uint64>& UserIds);

	/** Add conversation to cache */
	void AddConversationToCache(Uint64 InConversationId, const CArray<Uint64>& InUserIds);

	/**
	 * Add conversation to cache for specified user,
	 * @Note: called by AddConversationToCache
	 */
	void AddConversationsForUserToCache(const Uint64 InConversationId, const CArray<Uint64>& InUserIds);

protected:
	/** Map with conversations mapped into their data structs */
	CUnorderedMap<Uint64, std::shared_ptr<FConversationData>, Uint64> ConversationIdToConversationData;

	/** Mutex for ConversationIdToConversationData */
	std::shared_mutex ConversationIdToConversationDataMutex;

	/** Simple map with user conversations ids */
	CUnorderedMap<Uint64, FUserConversations, Uint64> UserIdToConversation;

	/** Mutex for UserIdToConversation */
	std::shared_mutex UserIdToConversationMutex;


};
