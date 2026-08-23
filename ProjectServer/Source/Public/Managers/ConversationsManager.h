#pragma once

#include "EngineCompat.h"
#include "Managers/MessageType.h"
#include <shared_mutex>

enum class EDatabaseOperationResult : Uint8;

namespace soci
{
	class session;
}

/** What is the status of certain message */
enum class EConversationMessageStatus : Uint8
{
	Sent = 0,
	Edited,
	Deleted,
};

/** Struct for conversation each message */
struct FConversationMessageData
{
	FConversationMessageData()
		: MessageId(0)
		, SenderId(0)
		, CreatedAt(0)
		, Status(EConversationMessageStatus::Sent)
		, MessageType(EMessageType::Text)
	{
	}

	Uint64 MessageId;

	/** Who sent message? */
	Uint64 SenderId;

	/** Actual message. For media types this holds the verified content hash. */
	std::string Message;

	/** Creation time as epoch nanoseconds (BIGINT UNSIGNED in DB) */
	Uint64 CreatedAt;

	/** Was edited */
	EConversationMessageStatus Status;

	/** What the message carries (text, image, gif, video). */
	EMessageType MessageType;

};

/** Struct for each conversation */
struct FConversationData
{
	Uint64 ConversationId;

	/** Users */
	CArray<Uint64> UsersIds;

	/** Map with user mapped to last read message id */
	CUnorderedMap<Uint64, Uint64> UserIdToMessageLastRead;

	/** Messages deque (Deque to used to easily get last X messages in range). */
	CDeque<FConversationMessageData> MessagesDeque;

	/** Lock for conversation access */
	std::shared_mutex Lock;

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
 *
 * @TODO: Async database queries would be way better.
 */
class FConversationsManager
{
public:
	/** Create new conversation */
	Uint64 GetOrCreateConversation(const CArray<Uint64>& InUserIds, bool& bIsNewConversation);

	std::shared_ptr<FConversationData> GetConversation(Uint64 InConversationId);
	bool HasConversation(Uint64 InConversationId);

	bool IsUserInConversation(Uint64 InUserId, Uint64 InConversationId);
	bool IsMessageInConversation(Uint64 InMessageId, Uint64 InConversationId);

	Uint64 AddMessage(Uint64 InConversationId, Uint64 InSenderId, const std::string& InMessage, EMessageType InMessageType = EMessageType::Text);
	void EditMessage(Uint64 InRequesterId, Uint64 InConversationId, Uint64 InMessageId, const std::string& InNewMessage);
	void DeleteMessage(Uint64 InRequesterId, Uint64 InConversationId, Uint64 InMessageId);

	void GetLastConversationByUserId(Uint64 InUserId, int32 Offset, int32 Limit, CArray<Uint64>& OutConversationIds);

	/**
	 * If messages are missing, it will attempt to download from DB
	 * @returns conversations
	 */
	std::vector<FConversationMessageData> GetConversationMessagesForRange(std::shared_ptr<FConversationData>& Conversation, int32 Offset, int32 Count);

private:
	/** Query DB for conversations of user */
	void DownloadConversationsFromRange(Uint64 UserId, int32 Offset, int32 Limit);

	/** Query DB for participants */
	void DownloadConversationParticipants(Uint64 InConversationId, CArray<Uint64>& OutUserIds, CArray<Uint64>& OutLastReadMessageIds);

	/** Query DB for messages */
	std::vector<FConversationMessageData> DownloadConversationMessages(Uint64 InConversationId, int32 InOffset, int32 InLimit);

	EDatabaseOperationResult UpdateMessageEditInDB(Uint64 RequesterUserId, Uint64 InConversationId, Uint64 InMessageId, const std::string& InNewMessage);
	EDatabaseOperationResult UpdateMessageDeleteInDB(Uint64 RequesterUserId, Uint64 InConversationId, Uint64 InMessageId);

	/**
	 * Conditional add conversation to DB
	 * If exists, we will not add another conversation, we will return previous one matching
	 */
	Uint64 UploadOrGetConversation(const std::vector<Uint64>& UserIds, bool& bIsNewConversation);

	/** UploadConversation helper for conversation between two people */
	Uint64 FindConversation2Ids(soci::session& Sql, Uint64 User1Id, Uint64 User2Id);

	/** Search for conversation with any amount of usersids */
	Uint64 FindConversationNIds(soci::session& Sql, const std::vector<Uint64>& UserIds);

	/** Add conversation to cache */
	void AddConversationToCache(Uint64 InConversationId, const CArray<Uint64>& InUserIds, const CArray<Uint64>& LastReadMessageIds);

	/**
	 * Add conversation to cache for specified user,
	 * @Note: called by AddConversationToCache
	 */
	void AddConversationsForUserToCache(const Uint64 InConversationId, const CArray<Uint64>& InUserIds);

	/** Send message to DB */
	EDatabaseOperationResult UploadMessage(Uint64 InConversationId, Uint64 SenderId, const std::string& InMessage, EMessageType InMessageType, Uint64& OutId);

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
