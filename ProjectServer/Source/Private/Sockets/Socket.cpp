#include "Sockets/Socket.h"

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "ProjectEngine.h"
#include "AbuseProtection/AbuseProtection.h"
#include "Auth/UserManager.h"
#include "DataBase/DataBaseConnect.h"
#include "Managers/ConversationsManager.h"
#include "Misc/WebSockets/CookieHelper.h"
#include "Sockets/WebSocketSessionData.h"

ESocketMessageType StringToSocketMessageType(const std::string& InTypeString)
{
	if (InTypeString == "send_message")				return ESocketMessageType::Message;
	if (InTypeString == "typing")					return ESocketMessageType::Typing;
	if (InTypeString == "mark_read")				return ESocketMessageType::MarkRead;
	if (InTypeString == "search_user")				return ESocketMessageType::SearchUser;
	if (InTypeString == "get_conversation")			return ESocketMessageType::GetConversation;
	if (InTypeString == "add_conversation")			return ESocketMessageType::AddConversation;
	if (InTypeString == "initial_client_data")		return ESocketMessageType::InitialClientData;
	if (InTypeString == "initial_conversations")	return ESocketMessageType::InitialConversations;
	if (InTypeString == "error")					return ESocketMessageType::Error;

	return ESocketMessageType::Unknown;
}

std::string SocketMessageTypeToString(const ESocketMessageType InTypeEnum)
{
	switch (InTypeEnum)
	{
		case ESocketMessageType::Message:				return "send_message";
		case ESocketMessageType::Typing:				return "typing";
		case ESocketMessageType::MarkRead:				return "mark_read";
		case ESocketMessageType::SearchUser:			return "search_user";
		case ESocketMessageType::GetConversation:		return "get_conversation";
		case ESocketMessageType::AddConversation:		return "add_conversation";
		case ESocketMessageType::InitialClientData:		return "initial_client_data";
		case ESocketMessageType::InitialConversations:	return "initial_conversations";
		case ESocketMessageType::Error:					return "error";

		case ESocketMessageType::Unknown:
		default:										return "unknown";
	}
}

FSocket::FSocket(const int32 InPort, bool bInUseSSL, const std::string& InKeyPath, const std::string& InCertPath)
	: Port(InPort)
	, bUseSSL(bInUseSSL)
	, SocketAppWrapper(bInUseSSL, InKeyPath, InCertPath)
	, AppListenSocket(nullptr)
{
	ProjectEngine = dynamic_cast<FProjectEngine*>(FGlobalDefines::GEngine);
}

FSocket::~FSocket()
{
	if (AppListenSocket != nullptr)
	{
		us_listen_socket_close(0, AppListenSocket);
	}
}

template<bool SSL>
auto CreateSocketBehavior(FSocket* Socket) {
	return typename uWS::TemplatedApp<SSL>::template WebSocketBehavior<FWebSocketSessionData>{
		/* Settings */
		.compression = uWS::SHARED_COMPRESSOR,
		.maxPayloadLength = 16 * 1024,
		.idleTimeout = 120, // Time in seconds
		.maxBackpressure = 1 * 1024 * 1024,
		/* Handlers */
		.upgrade = [](/** uWS::HttpResponse<SSL> */ auto* res, uWS::HttpRequest* req, auto* context)
		{
			auto CookieHeader = req->getHeader("cookie");
			const std::string AuthTokenValue = FCookieHelper::GetCookieValue(CookieHeader, "auth_token");

			/** Upgrade data used when connecting new client */
			struct FSocketUpgradeData
			{
				std::string secWebSocketKey;
				std::string secWebSocketProtocol;
				std::string secWebSocketExtensions;
				us_socket_context_t* context;
				decltype(res) httpRes;
				bool aborted = false;
			};

			auto upgradeData = std::make_shared<FSocketUpgradeData>(FSocketUpgradeData{
				.secWebSocketKey = std::string(req->getHeader("sec-websocket-key")),
				.secWebSocketProtocol = std::string(req->getHeader("sec-websocket-protocol")),
				.secWebSocketExtensions = std::string(req->getHeader("sec-websocket-extensions")),
				.context = context,
				.httpRes = res,
				.aborted = false
			});

			/* We have to attach an abort handler for us to be aware of disconnections while we perform task */
			res->onAborted([=]()
				{
					/* We don't implement any kind of cancellation here, so simply flag us as aborted */
					upgradeData->aborted = true;

					LOG_DEBUG("HTTP socket was closed before we upgraded it!");
				});

			bool bIsTokenValid = false;
			Uint64 TempUserId = 0;
			if (!AuthTokenValue.empty())
			{
				FProjectEngine* ProjectEngine = dynamic_cast<FProjectEngine*>(FGlobalDefines::GEngine);
				FUserManager* UserManager = ProjectEngine->GetUserManager();
				if (UserManager->VerifyToken(AuthTokenValue))
				{
					TempUserId = UserManager->GetIdFromToken(AuthTokenValue);
					bIsTokenValid = true;
				}
				else
				{
					std::string_view clientIp = res->getRemoteAddressAsText();

					// If behind proxy (nginx, cloudflare), check forwarded header
					std::string_view forwarded = req->getHeader("x-forwarded-for");
					if (!forwarded.empty())
					{
						clientIp = forwarded; // Use forwarded IP instead
					}

					ProjectEngine->GetAbuseProtection()->AddRateLimitedAttempt(std::string(clientIp));
				}
			}

			if (bIsTokenValid)
			{
				// Accept
				res->template upgrade<FWebSocketSessionData>(
					{ .UserId = TempUserId },  // userData
					req->getHeader("sec-websocket-key"),
					req->getHeader("sec-websocket-protocol"),
					req->getHeader("sec-websocket-extensions"),
					context
				);
			}
			else
			{
				res->writeStatus("401 Unauthorized");
				res->end("Invalid auth token");
				return;
			}
		},
		.open = [Socket](auto* ws)
		{
			Socket->OnClientConnected(ws);
		},
		.message = [Socket](auto* ws, std::string_view message, uWS::OpCode opCode)
		{
			Socket->OnMessageReceived(ws, message, opCode);
		},
		.close = [Socket](auto* ws, int code, std::string_view message)
		{
			Socket->OnClientDisconnected(ws, code, message);
		}
	};
}

void FSocket::Async()
{
	static const char* WebSocketPath = "/api/v1/ws";

	if (bUseSSL)
	{
		SocketAppWrapper.wsssl<FWebSocketSessionData>(WebSocketPath, CreateSocketBehavior<true>(this));
	}
	else
	{
		SocketAppWrapper.wssslno<FWebSocketSessionData>(WebSocketPath, CreateSocketBehavior<false>(this));
	}

	SocketAppWrapper.listen(Port, [&](us_listen_socket_t* listenSocket)
	{
		if (listenSocket != nullptr)
		{
			AppListenSocket = listenSocket;

			LOG_INFO("Server listening on port: " << Port);
		}
		else
		{
			LOG_ERROR("Failed to listen on port: " << Port);
		}
	});

	SocketAppWrapper.Run();
}

void FSocket::OnClientConnected(auto* ws)
{
#if DEBUG
	LOG_INFO("Client connected");
#endif

	FWebSocketSessionData* WebSocketSessionData = ws->getUserData();
	if (WebSocketSessionData != nullptr)
	{
		FUserManager* UserManger = ProjectEngine->GetUserManager();
		const Uint64 UserId = WebSocketSessionData->UserId;
		const FUser* User = UserManger->GetUser(UserId);
		if (User != nullptr)
		{
			// Add subscribe action
			{
				// SUBSCRIBE to a topic named after the user ID
				// syntax: "user_<id>"
				const std::string UserTopic = GenerateUserTopic(UserId);
				ws->subscribe(UserTopic);
			}

			// Send initial client data
			{
				nlohmann::json JsonData;
				JsonData["user_display_name"] = User->GetDisplayedName();

				nlohmann::json JsonRoot;
				JsonRoot["type"] = SocketMessageTypeToString(ESocketMessageType::InitialClientData);
				JsonRoot["data"] = JsonData;

				// Send initial client data
				ws->send(JsonRoot.dump(), uWS::TEXT);
			}
		}
		else
		{
			LOG_ERROR("Missing User!");
		}
	}
	else
	{
		LOG_ERROR("WebSocketSessionData is NULL!");
	}
}

void FSocket::OnClientDisconnected(auto* ws, int code, std::string_view message)
{
#if DEBUG
	LOG_INFO("Client disconnected");
#endif
}

void FSocket::OnMessageReceived(auto* ws, std::string_view message, uWS::OpCode opCode)
{
	switch (opCode)
	{
		case uWS::CONTINUATION:
		{
			break;
		}
		case uWS::TEXT:
		{
			OnMessageReceived_TEXT(ws, message, opCode);

			break;
		}
		case uWS::BINARY:
		{
			break;
		}
		case uWS::CLOSE:
		{
			break;
		}
		case uWS::PING:
		{
			OnMessageReceived_Ping(ws, message, opCode);

			break;
		}
		case uWS::PONG:
		{
			OnMessageReceived_Pong(ws, message, opCode);

			break;
		}
	}
}

void FSocket::OnMessageReceived_TEXT(auto* ws, std::string_view message, uWS::OpCode opCode)
{
#if DEBUG
	LOG_INFO("Received: " << message);
#endif

	try
	{
		nlohmann::json JsonMessage = nlohmann::json::parse(message);

		if (!JsonMessage.contains("type"))
		{
#if DEBUG
			LOG_ERROR("Message does not contain type");
#endif

			// Handle error
			return;
		}

		if (!JsonMessage.contains("data"))
		{
#if DEBUG
			LOG_ERROR("Message does not contain data");
#endif

			// Handle error
			return;
		}

		std::string SocketMessageTypeStr = JsonMessage["type"];
		const ESocketMessageType SocketMessage = StringToSocketMessageType(SocketMessageTypeStr);

		switch (SocketMessage)
		{
			case ESocketMessageType::Message:
			{
				if (JsonMessage["data"].contains("conversationId"))
				{
					const Uint64 ReceiverId = JsonMessage["data"]["receiverId"];
					const std::string Content = JsonMessage["data"]["content"];

					// Handle send message
					OnMessageReceived_SendMessage(ws, opCode, ReceiverId, Content);
				}

				break;
			}

			case ESocketMessageType::Typing:
			{
				if (JsonMessage["data"].contains("conversationId"))
				{
					const Uint64 ConversationId = JsonMessage["data"]["conversationId"];

					// Handle typing
					OnMessageReceived_Typing(ws, opCode, ConversationId);
				}

				break;
			}

			case ESocketMessageType::MarkRead:
			{
				if (JsonMessage["data"].contains("conversationId"))
				{
					Uint64 ConversationId = JsonMessage["data"]["conversationId"];

					// Handle mark read
					OnMessageReceived_MarkRead(ws, opCode, ConversationId);
				}

				break;
			}

			case ESocketMessageType::SearchUser:
			{
				if (JsonMessage["data"].contains("search_target"))
				{
					const std::string Pattern = JsonMessage["data"]["search_target"];

					OnMessageReceived_SearchUser(ws, opCode, Pattern);
				}

				break;
			}

			case ESocketMessageType::GetConversation:
			{
				FWebSocketSessionData* WebSocketSessionData = ws->getUserData();
				if (WebSocketSessionData != nullptr)
				{
					if (JsonMessage["data"].contains("offset") && JsonMessage["data"].contains("limit"))
					{
						const std::string OffsetString = JsonMessage["data"]["offset"];
						const std::string LimitString = JsonMessage["data"]["limit"];
						const int32 Offset = atoi(OffsetString.c_str());
						const int32 Limit = atoi(LimitString.c_str());

						OnMessageReceived_GetConversation(ws, opCode, WebSocketSessionData->UserId, Offset, Limit);
					}
				}

				break;
			}

			case ESocketMessageType::AddConversation:
			{
				if (JsonMessage["data"].contains("user_id"))
				{
					const std::string OtherUserIdAsString = JsonMessage["data"]["user_id"];
					const Uint64 OtherUserId = atoi(OtherUserIdAsString.c_str());

					OnMessageReceived_AddConversation(ws, opCode, OtherUserId);
				}

				break;
			}

			/** Errors */
			case ESocketMessageType::Unknown:
			default:
			{
				// Send error
				nlohmann::json ErrorJson;
				ErrorJson["type"] = SocketMessageTypeToString(ESocketMessageType::Error);
				ErrorJson["message"] = "Unknown message type";
				ws->send(ErrorJson.dump(), opCode);

				break;
			}
		}
	}
	catch (const nlohmann::json::exception& e)
	{
#if DEBUG
		LOG_ERROR("JSON error: " << e.what());
#endif
	}
}

void FSocket::OnMessageReceived_Ping(auto* ws, std::string_view message, uWS::OpCode opCode)
{
	if (opCode == uWS::PING)
	{
		ws->send(message, uWS::PONG);
	}
}

void FSocket::OnMessageReceived_Pong(auto* ws, std::string_view message, uWS::OpCode opCode)
{
	// We should not that connections is not dead
	ENSURE_VALID(false);
}

void FSocket::OnMessageReceived_SendMessage(auto* ws, uWS::OpCode opCode, const Uint64 ReceiverId, const std::string& Content)
{
}

void FSocket::OnMessageReceived_Typing(auto* ws, uWS::OpCode opCode, const Uint64 ConversationId)
{
}

void FSocket::OnMessageReceived_MarkRead(auto* ws, uWS::OpCode opCode, const Uint64 ConversationId)
{
}

void FSocket::OnMessageReceived_SearchUser(auto* ws, uWS::OpCode opCode, const std::string& Pattern)
{
	if (!Pattern.empty())
	{
		FDataBaseConnect Connect;
		if (Connect.IsConnected())
		{
			// Get database connection session
			soci::session& DataBaseSession = Connect.GetSession();

			static const Uint32 SearchResults = 20;

			std::string PatternQuery = "%" + Pattern + "%";

			struct FDataBaseUserStruct
			{
				Uint64 UserId;
				std::string UserName;
				std::string DisplayName;
			};

			std::vector<std::string> DisplayNames, UserNames;
			std::vector<Uint64> UserIds;

			DisplayNames.reserve(SearchResults);
			UserNames.reserve(SearchResults);
			UserIds.reserve(SearchResults);

			Uint64 UserId;
			std::string UserName, DisplayedName;

			soci::statement St = (DataBaseSession.prepare <<
				"SELECT id, username, displayedname "
				"FROM users "
				"WHERE displayedname LIKE :PatternQuery "
				"LIMIT 20",
				soci::into(UserId),      // Bind output variables
				soci::into(UserName),    // before execution
				soci::into(DisplayedName),
				soci::use(PatternQuery));

			St.execute();

			while (St.fetch())  // Fetch populates the bound variables
			{
				UserIds.push_back(UserId);
				UserNames.push_back(UserName);
				DisplayNames.push_back(DisplayedName);
			}

			const nlohmann::json UsersJson = FormatUsersToJson(UserIds, DisplayNames);
			const std::string JsonString = UsersJson.dump();

			nlohmann::json ReturnJson;
			ReturnJson["type"] = SocketMessageTypeToString(ESocketMessageType::SearchUser);
			ReturnJson["message"] = JsonString;
			ws->send(ReturnJson.dump(), opCode);
		}
	}
}

void FSocket::OnMessageReceived_GetConversation(auto* ws, uWS::OpCode opCode, Uint64 CurrentUserId, int32 Offset, int32 Limit)
{
	nlohmann::json JsonData;

	FDataBaseConnect Connect;
	if (Connect.IsConnected())
	{
		// Get database connection session
		soci::session& DataBaseSession = Connect.GetSession();

		const std::vector<FConversationInfo> RecentConversations = GetConversationsFromRange(DataBaseSession, CurrentUserId, Offset, Limit);

		nlohmann::json JsonRoot;
		JsonRoot["type"] = SocketMessageTypeToString(ESocketMessageType::InitialConversations);
		JsonRoot["data"] = JsonData;

		// Send initial client data
		ws->send(JsonRoot.dump(), uWS::TEXT);
	}
}

void FSocket::OnMessageReceived_AddConversation(auto* ws, uWS::OpCode opCode, const Uint64 OtherUserId)
{
	FWebSocketSessionData* WebSocketSessionData = ws->getUserData();
	if (WebSocketSessionData != nullptr && OtherUserId > 0)
	{
		FDataBaseConnect Connect;
		if (Connect.IsConnected())
		{
			// Get database connection session
			soci::session& DataBaseSession = Connect.GetSession();

			CArray<Uint64> UserIdArray;
			UserIdArray.Push(WebSocketSessionData->UserId);
			UserIdArray.Push(OtherUserId);

			const Uint64 ConversationId = AddConversation(DataBaseSession, UserIdArray.Vector);

			// Get conversation
			FConversationsManager* ConversationsManager = ProjectEngine->GetConversationsManager();
			std::shared_ptr<FConversationData> ConversationDataPtr = ConversationsManager->GetConversation(ConversationId);

			const nlohmann::json MessageJson = FormatConversationIntoJson(ConversationDataPtr);

			nlohmann::json ReturnJson;
			ReturnJson["type"] = SocketMessageTypeToString(ESocketMessageType::AddConversation);
			ReturnJson["message"] = MessageJson;
			ws->send(ReturnJson.dump(), opCode);
		}
	}
}

std::string FSocket::GenerateUserTopic(const Uint64 UserId)
{
	return "user_" + std::to_string(UserId);
}

nlohmann::json FSocket::FormatConversationIntoJson(std::shared_ptr<FConversationData>& ConversationDataPtr)
{
	nlohmann::json OutJson;

	FConversationData* ConversationData = ConversationDataPtr.get();
	OutJson["conversation_id"] = ConversationData->ConversationId;

	// Create participants json array
	{
		nlohmann::json ParticipantsJsonArray = nlohmann::json::array();

		for (const Uint64& UsersId : ConversationData->UsersIds)
		{
			nlohmann::json NewParticipant;

			NewParticipant["id"] = UsersId;

			ParticipantsJsonArray.push_back(NewParticipant);
		}

		OutJson["participants"] = ParticipantsJsonArray;
	}

	// Create messages array
	{
		nlohmann::json MessagesJsonArray = nlohmann::json::array();

		// @TODO get rid of magic '20'
		std::vector<FConversationMessageData> FrontMessages = ConversationData->MessagesMap.PeekFirst(20);
		for (const FConversationMessageData& FrontMessage : FrontMessages)
		{
			nlohmann::json NewMessage;

			NewMessage["id"] = FrontMessage.MessageId;
			NewMessage["sender_id"] = FrontMessage.SenderId;
			NewMessage["message"] = FrontMessage.Message;

			MessagesJsonArray.push_back(NewMessage);
		}

		OutJson["messages"] = MessagesJsonArray;
	}

	return OutJson;
}

nlohmann::json FSocket::FormatUsersToJson(const std::vector<uint64_t>& UserIds, const std::vector<std::string>& DisplayNames)
{
	nlohmann::json DataUserArray = nlohmann::json::array();

	for (size_t i = 0; i < UserIds.size(); i++)
	{
		nlohmann::json SingleUser = {
			{"id", UserIds[i]},
			{"displayName", DisplayNames[i]},
			{"status", "online"}
		};

		DataUserArray.push_back(SingleUser);
	}

	// Standard JSON with root object
	nlohmann::json JsonRoot;
	JsonRoot["data"] = DataUserArray;
	return JsonRoot;
}

Uint64 FSocket::AddConversation(soci::session& DataBaseSession, const std::vector<Uint64>& UserIds)
{
	// Declare ConversationId properly
	long long ConversationId = 0;

	// Special case for 2 users direct message which we do not want to duplicate
	if (UserIds.size() == 2)
	{
		const Uint64 FoundId = FindDirectConversation(DataBaseSession, UserIds[0], UserIds[1]);
		if (FoundId > 0)
		{
			// Skip search
			return FoundId;
		}
	}

	try
	{
		DataBaseSession << "INSERT INTO conversations (last_message_at) "
			"VALUES (CURRENT_TIMESTAMP)";
	}
	catch (const soci::soci_error& e)
	{
		LOG_ERROR("Database error: " << e.what());
	}

	try
	{
		// Get auto-generated ID - check if this actually works
		DataBaseSession.get_last_insert_id("conversations", ConversationId);
	}
	catch (const soci::soci_error& e)
	{
		LOG_ERROR("Database error: " << e.what());
	}

	try
	{
		// Add participants - make sure types match
		for (Uint64 UserId : UserIds)
		{
			long UserIdLong = static_cast<long>(UserId);
			DataBaseSession << "INSERT INTO conversation_participants "
				"(conversation_id, user_id) "
				"VALUES (:convId, :userId)",
				soci::use(ConversationId), soci::use(UserIdLong);
		}
	}
	catch (const soci::soci_error& e)
	{
		LOG_ERROR("Database error: " << e.what());
	}

	if (ConversationId > 0)
	{
		FConversationsManager* ConversationsManager = ProjectEngine->GetConversationsManager();
		ConversationsManager->AddConversation(ConversationId, UserIds);
	}

	return ConversationId;
}

Uint64 FSocket::FindDirectConversation(soci::session& Sql, Uint64 User1Id, Uint64 User2Id)
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

	return static_cast<Uint64>(ConversationId);
}

std::vector<FConversationInfo> FSocket::GetConversationsFromRange(soci::session& Sql, Uint64 UserId, int32 Offset, int32 Limit)
{
	std::vector<FConversationInfo> Conversations;

	try
	{
		FConversationInfo Info;

		soci::statement St = (Sql.prepare <<
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
			soci::into(Info.LastReadMessageId));

		St.execute();

		while (St.fetch())
		{
			Conversations.push_back(Info);
		}
	}
	catch (const soci::soci_error& e)
	{
		LOG_ERROR("Database error: " << e.what());
	}

	return Conversations;
}
