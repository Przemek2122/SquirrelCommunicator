#include "Managers/ConversationsManager.h"
#include "DataBase/DataBaseConnect.h"

Uint64 FConversationsManager::GetOrCreateConversation(const CArray<Uint64>& InUserIds)
{
	return UploadOrGetConversation(InUserIds.Vector);
}

std::shared_ptr<FConversationData> FConversationsManager::GetConversation(const Uint64 InConversationId)
{
	std::shared_ptr<FConversationData> ConversationDataSharedPtr;
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

void FConversationsManager::DownloadConversationsFromRange(const Uint64 UserId, const int32 Offset, const int32 Limit)
{
	std::vector<FConversationInfo> Conversations;

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
				// Check if the value was NULL
				if (ReadMsgInd == soci::i_null) {
					Info.LastReadMessageId = 0; // Set a default value (e.g. 0 or -1)
				}

				Conversations.push_back(Info);
			}
		}
	}
	catch (const soci::soci_error& e)
	{
		LOG_ERROR("Database error in GetConversations: " << e.what());
	}

	for (FConversationInfo& Conversation : Conversations)
	{
		// @TODO: Field of optimization query, instead of per conversation request could be one big.
		CArray<Uint64> Participants;
		DownloadConversationParticipants(Conversation.ConversationId, Participants);

		AddConversationToCache(Conversation.ConversationId, Participants);
	}
}

void FConversationsManager::DownloadConversationParticipants(Uint64 InConversationId, CArray<Uint64>& OutUserIds)
{
    // 1. Clear previous data
    OutUserIds.Clear();

    FDataBaseConnect Connect;
    if (Connect.IsConnected())
    {
        soci::session& DataBaseSession = Connect.GetSession();

        try
        {
            Uint64 FetchedUserId;

            // 2. Prepare Statement
            // We only need user_id from the join table
            soci::statement St = (DataBaseSession.prepare <<
                "SELECT user_id "
                "FROM conversation_participants "
                "WHERE conversation_id = :convId",
                soci::use(InConversationId),
                soci::into(FetchedUserId)
                );

            // 3. Execute
            St.execute();

            // 4. Fetch Loop
            while (St.fetch())
            {
                OutUserIds.Push(FetchedUserId);
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

Uint64 FConversationsManager::UploadOrGetConversation(const std::vector<Uint64>& UserIds)
{
	// Declare ConversationId properly
	long long ConversationId = 0;

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

			AddConversationToCache(ConversationId, UserIds);
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
	AddConversationToCache(ConversationId, { User1Id, User2Id });

	return static_cast<Uint64>(ConversationId);
}

Uint64 FConversationsManager::FindConversationNIds(soci::session& Sql, const std::vector<Uint64>& UserIds)
{
	Uint64 OutId = 0;

	// @TODO for later, we need direct conversations first
	LOG_WARN("Missing check for creating groups");

	return OutId;
}

void FConversationsManager::AddConversationToCache(const Uint64 InConversationId, const CArray<Uint64>& InUserIds)
{
	ConversationIdToConversationDataMutex.lock();
	if (!ConversationIdToConversationData.ContainsKey(InConversationId))
	{
		std::shared_ptr<FConversationData> ConversationDataSharedPtr = std::make_shared<FConversationData>();
		ConversationDataSharedPtr->ConversationId = InConversationId;
		ConversationDataSharedPtr->UsersIds = InUserIds;

		ConversationIdToConversationData.InsertOrAssign(InConversationId, ConversationDataSharedPtr);
		ConversationIdToConversationDataMutex.unlock();

		AddConversationsForUserToCache(InConversationId, InUserIds);
	}
}

void FConversationsManager::AddConversationsForUserToCache(const Uint64 InConversationId, const CArray<Uint64>& InUserIds)
{
	const std::unique_lock UniqueLock(UserIdToConversationMutex);
	for (const Uint64& InUserId : InUserIds)
	{
		if (!UserIdToConversation.ContainsKey(InUserId))
		{
			FUserConversations& User = UserIdToConversation[InUserId];
			if (!User.Conversations.Contains(InConversationId))
			{
				User.Conversations.Push(InConversationId);
			}
		}
	}
}
