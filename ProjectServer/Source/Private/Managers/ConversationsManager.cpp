#include "Logger/Logger.h"
#include "Managers/ConversationsManager.h"
#include "DataBase/DataBaseConnect.h"
#include "soci/rowset.h"

Uint64 FConversationsManager::GetOrCreateConversation(const CArray<Uint64>& InUserIds, bool& bIsNewConversation)
{
	return UploadOrGetConversation(InUserIds.Vector(), bIsNewConversation);
}

std::shared_ptr<FConversationData> FConversationsManager::GetConversation(const Uint64 InConversationId)
{
	std::shared_ptr<FConversationData> ConversationDataSharedPtr = nullptr;
	std::shared_lock Lock(ConversationIdToConversationDataMutex);

	if (ConversationIdToConversationData.ContainsKey(InConversationId))
	{
		ConversationDataSharedPtr = ConversationIdToConversationData[InConversationId];
	}

	return ConversationDataSharedPtr;
}

bool FConversationsManager::HasConversation(const Uint64 InConversationId)
{
	std::shared_lock Lock(ConversationIdToConversationDataMutex);

	return (ConversationIdToConversationData.ContainsKey(InConversationId));
}

bool FConversationsManager::IsUserInConversation(const Uint64 InUserId, const Uint64 InConversationId)
{
	const bool bConversationExists = HasConversation(InConversationId);
	if (bConversationExists)
	{
		std::shared_ptr<FConversationData> ConversationDataSharedPtr;

		// Get conversation data with lock
		{
			std::shared_lock Lock(ConversationIdToConversationDataMutex);
			ConversationDataSharedPtr = ConversationIdToConversationData[InConversationId];
		}

		// Access data with lock
		{
			std::shared_lock Lock(ConversationDataSharedPtr->Lock);
			return ConversationDataSharedPtr->UsersIds.Contains(InUserId);
		}
	}

	return false;
}

bool FConversationsManager::IsMessageInConversation(Uint64 InMessageId, Uint64 InConversationId)
{
	const bool bConversationExists = HasConversation(InConversationId);
	if (bConversationExists)
	{
		std::shared_ptr<FConversationData> ConversationDataSharedPtr;

		// Get conversation data with lock
		{
			std::shared_lock Lock(ConversationIdToConversationDataMutex);
			ConversationDataSharedPtr = ConversationIdToConversationData[InConversationId];
		}

		// Access data with lock
		// Linear search from the end
		{
			std::shared_lock Lock(ConversationDataSharedPtr->Lock);
			const std::deque<FConversationMessageData>& Deque = ConversationDataSharedPtr->MessagesDeque.Deque();

			uint16 CheckedCount = 0;
			const uint16 MaxLookback = 50;

			// Go from latest messages
			for (auto It = Deque.begin(); It != Deque.end(); ++It)
			{
				if (It->MessageId == InMessageId)
				{
					return true; // Found
				}

				CheckedCount++;

				// We only allow to edit last 50? Message
				// tbh 50 is generous anyway
				if (It->MessageId < InMessageId || CheckedCount > MaxLookback)
				{
					return false;
				}
			}
		}
	}

	return false;
}

Uint64 FConversationsManager::AddMessage(const Uint64 InConversationId, const Uint64 InSenderId, const std::string& InMessage)
{
	std::shared_ptr<FConversationData> ConversationPtr = GetConversation(InConversationId);

	Uint64 OutId = 0;
	const EDatabaseOperationResult Result = UploadMessage(InConversationId, InSenderId, InMessage, OutId);
	if (Result == EDatabaseOperationResult::Success)
	{
		FConversationMessageData ConversationMessageData;
		ConversationMessageData.MessageId = OutId;
		ConversationMessageData.SenderId = InSenderId;
		ConversationMessageData.Message = InMessage;
		ConversationMessageData.CreatedAt = static_cast<Uint64>(std::chrono::system_clock::now().time_since_epoch().count());

		// Lock conversation for adding message
		std::unique_lock Lock(ConversationPtr->Lock);

		// Add message at begging of table as it's newest
		ConversationPtr->MessagesDeque.PushFront(ConversationMessageData);
	}

	return OutId;
}

void FConversationsManager::EditMessage(const Uint64 InRequesterId, const Uint64 InConversationId, const Uint64 InMessageId, const std::string& InNewMessage)
{
    // 1. Database Update First
    const EDatabaseOperationResult Result = UpdateMessageEditInDB(InRequesterId, InConversationId, InMessageId, InNewMessage);

    if (Result == EDatabaseOperationResult::Success)
    {
        std::shared_ptr<FConversationData> ConversationDataSharedPtr;

        // 2. Find the conversation (Safe read from the global map)
        {
            std::shared_lock Lock(ConversationIdToConversationDataMutex);
            auto ConvIt = ConversationIdToConversationData.Map().find(InConversationId);
            if (ConvIt == ConversationIdToConversationData.end() || ConvIt->second == nullptr)
            {
                return; // Conversation does not exist
            }
            ConversationDataSharedPtr = ConvIt->second;
        }

        // 3. Edit the message (WRITE lock at the conversation level)
        {
            // Using unique_lock! No other thread can read or write to this deque right now.
            std::unique_lock WriteLock(ConversationDataSharedPtr->Lock);
            std::deque<FConversationMessageData>& Deque = ConversationDataSharedPtr->MessagesDeque.Deque();

        	int32 CurrentLoopback = 0;
        	const int32 MaxLoopback = 50;

            // Reverse linear search, because users usually edit recent messages.
            for (auto It = Deque.begin(); It != Deque.end(); ++It)
            {
                if (It->MessageId == InMessageId)
                {
                    // Found it! Replace the content.
                    It->Message = InNewMessage;
                    It->Status = EConversationMessageStatus::Edited;

                    break;
                }

                // Optimization: If we hit a smaller ID, the message cannot be here anymore.
                if (It->MessageId < InMessageId || CurrentLoopback >= MaxLoopback)
                {
                    break;
                }
            }
        }

        // Remember to broadcast change when using this function
    }
}

void FConversationsManager::DeleteMessage(const Uint64 InRequesterId, const Uint64 InConversationId, const Uint64 InMessageId)
{
    // 1. Database Update First
    const EDatabaseOperationResult Result = UpdateMessageDeleteInDB(InRequesterId, InConversationId, InMessageId);

    if (Result == EDatabaseOperationResult::Success)
    {
        std::shared_ptr<FConversationData> ConversationDataSharedPtr;

        // 2. Find the conversation
        {
            std::shared_lock Lock(ConversationIdToConversationDataMutex);
            auto ConvIt = ConversationIdToConversationData.Map().find(InConversationId);
            if (ConvIt == ConversationIdToConversationData.end() || ConvIt->second == nullptr)
            {
                return;
            }
            ConversationDataSharedPtr = ConvIt->second;
        }

        // 3. Delete the message (WRITE lock at the conversation level)
        {
            std::unique_lock WriteLock(ConversationDataSharedPtr->Lock);
            std::deque<FConversationMessageData>& Deque = ConversationDataSharedPtr->MessagesDeque.Deque();

            auto It = std::lower_bound(
                Deque.begin(),
                Deque.end(),
                InMessageId,
                [](const FConversationMessageData& Msg, const Uint64 Id) {
                    return Msg.MessageId < Id;
                }
            );

            if (It != Deque.end() && It->MessageId == InMessageId)
            {
                // std::deque::erase removes the element and shifts the rest.
                // This is a relatively fast operation for std::deque.
                Deque.erase(It);
            }
        }

        // Remember to broadcast change when using this function
    }
}
void FConversationsManager::GetLastConversationByUserId(const Uint64 InUserId, const int32 Offset, const int32 Limit, CArray<Uint64>& OutConversationIds)
{
	// @TODO We could for sure optimize this.
	// Download everything before up to the requested point
	DownloadConversationsFromRange(InUserId, 0, Offset + Limit);

	const std::shared_lock SharedLock(UserIdToConversationMutex);
	if (UserIdToConversation.ContainsKey(InUserId))
	{
		FUserConversations& UserConversations = UserIdToConversation[InUserId];
		const int32 Start = Offset > 1 ? Offset - 1 : 0;
		for (int32 i = Start; i < UserConversations.Conversations.Size(); i++)
		{
			OutConversationIds.Push(UserConversations.Conversations[i]);
		}
	}
}

std::vector<FConversationMessageData> FConversationsManager::GetConversationMessagesForRange(std::shared_ptr<FConversationData>& Conversation, int32 Offset, int32 Count)
{
	std::vector<FConversationMessageData> ConversationMessage;

	const int32 CurrentMessagesCount = Conversation->MessagesDeque.Size();
	const int32 TargetMessagesCount = Offset + Count;
	if (CurrentMessagesCount < TargetMessagesCount)
	{
		const int32 TargetOffset = CurrentMessagesCount;
		const int32 TargetLimit = TargetMessagesCount - CurrentMessagesCount;

		// If missing try query DB
		ConversationMessage = DownloadConversationMessages(Conversation->ConversationId, TargetOffset, TargetLimit);

		// Add messages to cache
		for (const FConversationMessageData& Message : ConversationMessage)
		{
			Conversation->MessagesDeque.PushBack(Message);
		}

		// Add any present in memory but skipped in download
		if (TargetOffset != Offset)
		{
			std::vector<FConversationMessageData> ConversationMessageInMemoryPart = Conversation->MessagesDeque.GetRange(Offset, TargetOffset);

			for (FConversationMessageData& MessageInMemoryPart : ConversationMessageInMemoryPart)
			{
				ConversationMessage.push_back(MessageInMemoryPart);
			}
		}
	}
	else
	{
		ConversationMessage = Conversation->MessagesDeque.GetRange(Offset, Count);
	}

	return ConversationMessage;
}

void FConversationsManager::DownloadConversationsFromRange(const Uint64 UserId, const int32 Offset, const int32 Limit)
{
	CArray<FConversationInfo> ConversationArray;

	try
	{
		FDataBaseConnect Connect;
		if (Connect.IsConnected())
		{
			// Get database connection session
			soci::session& DataBaseSession = Connect.GetSession();

			FConversationInfo Info;

			// We need an indicator variable to detect NULLs
			soci::indicator ReadMsgInd;

			soci::statement St = (DataBaseSession.prepare <<
				"SELECT c.conversation_id, c.last_message_at, cp.last_read_message_id "
				"FROM conversations c "
				"JOIN conversation_participants cp ON c.conversation_id = cp.conversation_id "
				"WHERE cp.user_id = :userId "
				"ORDER BY c.last_message_at DESC "
				"LIMIT :limit OFFSET :offset",
				soci::use(UserId),
				soci::use(Limit),
				soci::use(Offset),
				soci::into(Info.ConversationId),
				soci::into(Info.LastMessageAt),
				soci::into(Info.LastReadMessageId, ReadMsgInd)
			);

			St.execute();

			while (St.fetch())
			{
				ConversationArray.Push(Info);
			}
		}
	}
	catch (const soci::soci_error& e)
	{
		LOG_ERROR("Database error in GetConversations: " << e.what());
	}

	// Remove already existing conversations
	{
		std::shared_lock<std::shared_mutex> MutexSharedScopedLock(ConversationIdToConversationDataMutex);

		for (uint32 i = 0; i < ConversationArray.Size(); i++)
		{
			const FConversationInfo& Conversation = ConversationArray[i];
			if (ConversationIdToConversationData.ContainsKey(Conversation.ConversationId))
			{
				ConversationArray.RemoveAt(i);
				i--;
			}
		}
	}

	for (FConversationInfo& Conversation : ConversationArray)
	{
		// @TODO: Field of optimization query, instead of per conversation request could be one big.
		CArray<Uint64> Participants;
		CArray<Uint64> LastReadMessageIds;
		DownloadConversationParticipants(Conversation.ConversationId, Participants, LastReadMessageIds);

		AddConversationToCache(Conversation.ConversationId, Participants, LastReadMessageIds);

		const std::shared_ptr<FConversationData> ConversationPtr = GetConversation(Conversation.ConversationId);

		std::vector<FConversationMessageData> Messages;
		{
			std::shared_lock<std::shared_mutex> Lock(ConversationPtr->Lock);
			Messages = DownloadConversationMessages(Conversation.ConversationId, 0, 40);
		}

		{
			std::unique_lock Lock(ConversationPtr->Lock);
			for (const FConversationMessageData& Message : Messages)
			{
				ConversationPtr->MessagesDeque.PushBack(Message);
			}
		}
	}
}

void FConversationsManager::DownloadConversationParticipants(Uint64 InConversationId, CArray<Uint64>& OutUserIds, CArray<Uint64>& OutLastReadMessageIds)
{
    // 1. Clear previous data
	OutUserIds.Clear();
	OutLastReadMessageIds.Clear();

    FDataBaseConnect Connect;
    if (Connect.IsConnected())
    {
        soci::session& DataBaseSession = Connect.GetSession();

        try
        {
			Uint64 FetchedUserId;
			Uint64 FetchedLastReadMessageId;
			soci::indicator FetchedLastReadMessageIdInd;

            // 2. Prepare Statement
            // We only need user_id from the join table
            soci::statement St = (DataBaseSession.prepare <<
                "SELECT user_id, last_read_message_id  "
                "FROM conversation_participants "
                "WHERE conversation_id = :convId",
                soci::use(InConversationId),
				soci::into(FetchedUserId),
				soci::into(FetchedLastReadMessageId, FetchedLastReadMessageIdInd)
            );

            // 3. Execute
            St.execute();

            // 4. Fetch Loop
            while (St.fetch())
            {
				if (FetchedLastReadMessageIdInd == soci::i_null)
				{
					FetchedLastReadMessageId = 0;
				}

				OutUserIds.Push(FetchedUserId);
				OutLastReadMessageIds.Push(FetchedLastReadMessageId);
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Database error (DownloadConversationParticipants): " << e.what());
        }
    }
    else
    {
        LOG_ERROR("Database connection failed in DownloadConversationParticipants");
    }
}

std::vector<FConversationMessageData> FConversationsManager::DownloadConversationMessages(const Uint64 InConversationId, const int32 InOffset, const int32 InLimit)
{
	std::vector<FConversationMessageData> ConversationData;

	FDataBaseConnect Connect;
	if (Connect.IsConnected())
	{
		try
		{
			soci::session& DataBaseSession = Connect.GetSession();

			Uint64 MessageId;
			Uint64 SenderId;
			std::string MessageText;
			long long CreatedAtDb = 0;

			soci::statement Stmt = (DataBaseSession.prepare <<
				"SELECT id, sender_id, text, created_at FROM messages WHERE conversation_id = :conv_id ORDER BY id DESC LIMIT :limit OFFSET :offset",
				soci::into(MessageId),
				soci::into(SenderId),
				soci::into(MessageText),
				soci::into(CreatedAtDb),
				soci::use(InConversationId),
				soci::use(InLimit),
				soci::use(InOffset));

			Stmt.execute();

			std::unique_lock Lock(ConversationIdToConversationDataMutex);

			while (Stmt.fetch())
			{
				FConversationMessageData MessageData;
				MessageData.MessageId = MessageId;
				MessageData.SenderId = SenderId;
				MessageData.Message = MessageText;
				MessageData.CreatedAt = static_cast<Uint64>(CreatedAtDb);

				ConversationData.push_back(MessageData);
			}
		}
		catch (const soci::soci_error& Error)
		{
			LOG_ERROR("Database error: " << Error.what());
		}
	}

	return ConversationData;
}

EDatabaseOperationResult FConversationsManager::UpdateMessageEditInDB(const Uint64 RequesterUserId, const Uint64 InConversationId, const Uint64 InMessageId, const std::string& InNewMessage)
{
	try
	{
		FDataBaseConnect Connect;
		if (Connect.IsConnected())
		{
			soci::session& DataBaseSession = Connect.GetSession();

			// cast to int for safety
			int32 StatusInt = static_cast<int32>(EConversationMessageStatus::Edited);

			soci::statement St = (DataBaseSession.prepare <<
				"UPDATE messages SET text = :text, text_status = :status "
				"WHERE id = :msgId AND conversation_id = :convId AND sender_id = :senderId",
				soci::use(InNewMessage, "text"),
				soci::use(StatusInt, "status"),
				soci::use(InMessageId, "msgId"),
				soci::use(InConversationId, "convId"),
				soci::use(RequesterUserId, "senderId")
			);

			St.execute();

			// Check how many rows were affected
			if (St.get_affected_rows() == 0)
			{
				LOG_ERROR("No rows affected");

				return EDatabaseOperationResult::DatabaseFailed;
			}

			return EDatabaseOperationResult::Success;
		}
	}
	catch (const soci::soci_error& e)
	{
		LOG_ERROR("Database error during message edit: " << e.what());

		return EDatabaseOperationResult::OperationFailed;
	}

	return EDatabaseOperationResult::Unknown;
}

EDatabaseOperationResult FConversationsManager::UpdateMessageDeleteInDB(const Uint64 RequesterUserId, const Uint64 InConversationId, const Uint64 InMessageId)
{
	try
	{
		FDataBaseConnect Connect;
		if (Connect.IsConnected())
		{
			soci::session& DataBaseSession = Connect.GetSession();

			int32 StatusInt = static_cast<int32>(EConversationMessageStatus::Deleted);

			std::string EmptyText = "";

			soci::statement St = (DataBaseSession.prepare <<
				"UPDATE messages SET "
				"text = :text, "
				"text_status = :status "
				"WHERE id = :msgId AND conversation_id = :convId AND sender_id = :senderId",
				soci::use(EmptyText, "text"),
				soci::use(StatusInt, "status"),
				soci::use(InMessageId, "msgId"),
				soci::use(InConversationId, "convId"),
				soci::use(RequesterUserId, "senderId")
			);

			St.execute();

			return EDatabaseOperationResult::Success;
		}
	}
	catch (const soci::soci_error& e)
	{
		LOG_ERROR("Database error during message deletion: " << e.what());

		return EDatabaseOperationResult::OperationFailed;
	}

	return EDatabaseOperationResult::Unknown;
}

Uint64 FConversationsManager::UploadOrGetConversation(const std::vector<Uint64>& UserIds, bool& bIsNewConversation)
{
	// Declare ConversationId properly
	long long ConversationId = 0;
	bIsNewConversation = false;

	FDataBaseConnect Connect;
	if (Connect.IsConnected())
	{
		// Get database connection session
		soci::session& DataBaseSession = Connect.GetSession();

		// Special case for 2 users direct message which we do not want to duplicate
		if (UserIds.size() == 2)
		{
			const Uint64 FoundId = FindConversation2Ids(DataBaseSession, UserIds[0], UserIds[1]);
			if (FoundId > 0)
			{
				// Skip search
				return FoundId;
			}
		}
		else
		{
			const Uint64 FoundId = FindConversationNIds(DataBaseSession, UserIds);
			if (FoundId > 0)
			{
				// Skip search
				return FoundId;
			}
		}

		// Create new conversation
		try
		{
			DataBaseSession << "INSERT INTO conversations (last_message_at) "
				"VALUES (CURRENT_TIMESTAMP)";

			bIsNewConversation = true;
		}
		catch (const soci::soci_error& e)
		{
			LOG_ERROR("Database error: " << e.what());
		}

		// Get auto-generated ID
		try
		{
			DataBaseSession.get_last_insert_id("conversations", ConversationId);
		}
		catch (const soci::soci_error& e)
		{
			LOG_ERROR("Database error: " << e.what());
		}

		if (ConversationId > 0)
		{
			// Add participants - make sure types match
			try
			{
				for (Uint64 UserId : UserIds)
				{
					long UserIdLong = static_cast<long>(UserId);
					DataBaseSession << "INSERT INTO conversation_participants "
						"(conversation_id, user_id) "
						"VALUES (:convId, :userId)",
						soci::use(ConversationId),
						soci::use(UserIdLong);
				}
			}
			catch (const soci::soci_error& e)
			{
				LOG_ERROR("Database error: " << e.what());
			}

			CArray<Uint64> Ids;
			Ids.resize(UserIds.size());
			CArray<Uint64> UserIdsCArray(UserIds.begin(), UserIds.end());
			AddConversationToCache(ConversationId, UserIdsCArray, Ids);
		}
		else
		{
			LOG_ERROR("ConversationId not found.");
		}
	}

	return ConversationId;
}

Uint64 FConversationsManager::FindConversation2Ids(soci::session& Sql, Uint64 User1Id, Uint64 User2Id)
{
	long long ConversationId = 0;

	try
	{
		Sql << "SELECT cp1.conversation_id "
			"FROM conversation_participants cp1 "
			"INNER JOIN conversation_participants cp2 "
			"    ON cp1.conversation_id = cp2.conversation_id "
			"WHERE cp1.user_id = :user1 "
			"AND cp2.user_id = :user2 "
			"AND cp1.conversation_id IN ("
			"    SELECT conversation_id "
			"    FROM conversation_participants "
			"    GROUP BY conversation_id "
			"    HAVING COUNT(*) = 2"
			")",
			soci::use(User1Id, "user1"),
			soci::use(User2Id, "user2"),
			soci::into(ConversationId);
	}
	catch (const soci::soci_error& e)
	{
		LOG_ERROR("Database error: " << e.what());
	}

	// Add to cache if missing
	AddConversationToCache(ConversationId, { User1Id, User2Id }, { 0, 0 });

	return static_cast<Uint64>(ConversationId);
}

Uint64 FConversationsManager::FindConversationNIds(soci::session& Sql, const std::vector<Uint64>& UserIds)
{
	Uint64 OutId = 0;

	// @TODO for later, we need direct conversations first
	LOG_WARN("Missing check for creating groups");

	return OutId;
}

void FConversationsManager::AddConversationToCache(const Uint64 InConversationId, const CArray<Uint64>& InUserIds, const CArray<Uint64>& LastReadMessageIds)
{
	ConversationIdToConversationDataMutex.lock();
	if (!ConversationIdToConversationData.ContainsKey(InConversationId))
	{
		std::shared_ptr<FConversationData> ConversationDataSharedPtr = std::make_shared<FConversationData>();
		ConversationDataSharedPtr->ConversationId = InConversationId;
		ConversationDataSharedPtr->UsersIds = InUserIds;

		for (int32 i = 0; i < InUserIds.Size(); i++)
		{
			ConversationDataSharedPtr->UserIdToMessageLastRead[InUserIds[i]] = LastReadMessageIds[i];
		}

		ConversationIdToConversationData.InsertOrAssign(InConversationId, ConversationDataSharedPtr);
		ConversationIdToConversationDataMutex.unlock();

		AddConversationsForUserToCache(InConversationId, InUserIds);
	}
	else
	{
		ConversationIdToConversationDataMutex.unlock();
	}
}

void FConversationsManager::AddConversationsForUserToCache(const Uint64 InConversationId, const CArray<Uint64>& InUserIds)
{
	const std::unique_lock UniqueLock(UserIdToConversationMutex);
	for (const Uint64& InUserId : InUserIds)
	{
		// Get or create
		FUserConversations& User = UserIdToConversation[InUserId];

		// Add if missing
		if (!User.Conversations.Contains(InConversationId))
		{
			User.Conversations.Push(InConversationId);
		}
	}
}

EDatabaseOperationResult FConversationsManager::UploadMessage(const Uint64 InConversationId, const Uint64 SenderId, const std::string& InMessage, Uint64& OutId)
{
	EDatabaseOperationResult DatabaseOperationResult = EDatabaseOperationResult::Unknown;

	FDataBaseConnect Connect;
	if (Connect.IsConnected())
	{
		try
		{
			// Get database connection session
			soci::session& DataBaseSession = Connect.GetSession();
			soci::indicator Ind;

			const Uint64 NowNanos = static_cast<Uint64>(std::chrono::system_clock::now().time_since_epoch().count());
			DataBaseSession << "INSERT INTO messages (conversation_id, sender_id, text, created_at) VALUES (:conv_id, :sender_id, :text, :created)",
				soci::use(InConversationId),
				soci::use(SenderId),
				soci::use(InMessage, Ind),
				soci::use(NowNanos);

			// Get last inserted ID
			long long LastInsertId;
			DataBaseSession << "SELECT LAST_INSERT_ID()", soci::into(LastInsertId);
			OutId = static_cast<Uint64>(LastInsertId);

			DataBaseSession << "UPDATE conversations SET last_message_at = NOW() WHERE conversation_id = :conv_id",
				soci::use(InConversationId);

			if (OutId > 0)
			{
				DatabaseOperationResult = EDatabaseOperationResult::Success;
			}
			else
			{
				DatabaseOperationResult = EDatabaseOperationResult::OperationFailed;
			}
		}
		catch (const soci::soci_error& Error)
		{
			// Handle SOCI error
			LOG_ERROR("Database error: " << Error.what());

			DatabaseOperationResult = EDatabaseOperationResult::DatabaseFailed;
		}
	}
	else
	{
		DatabaseOperationResult = EDatabaseOperationResult::ConnectionFailed;
	}

	return DatabaseOperationResult;
}
