#include "Sockets/Socket.h"

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "ProjectEngine.h"
#include "AbuseProtection/AbuseProtection.h"
#include "Auth/UserManager.h"
#include "DataBase/DataBaseConnect.h"
#include "Managers/ConversationsManager.h"
#include "Misc/WebSockets/CookieHelper.h"
#include "Sockets/SocketManager.h"
#include "Sockets/WebSocketSessionData.h"

ESocketMessageType StringToSocketMessageType(const std::string& InTypeString)
{
	if (InTypeString == "message")					return ESocketMessageType::Message;
	if (InTypeString == "typing")					return ESocketMessageType::Typing;
	if (InTypeString == "mark_read")				return ESocketMessageType::MarkRead;
	if (InTypeString == "user_status")				return ESocketMessageType::UserStatus;
	if (InTypeString == "search_user")				return ESocketMessageType::SearchUser;
	if (InTypeString == "load_more_messages")		return ESocketMessageType::LoadMoreMessages;
	if (InTypeString == "get_conversations")		return ESocketMessageType::GetConversations;
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
		case ESocketMessageType::Message:				return "message";
		case ESocketMessageType::Typing:				return "typing";
		case ESocketMessageType::MarkRead:				return "mark_read";
		case ESocketMessageType::UserStatus:			return "user_status";
		case ESocketMessageType::SearchUser:			return "search_user";
		case ESocketMessageType::LoadMoreMessages:		return "load_more_messages";
		case ESocketMessageType::GetConversations:		return "get_conversations";
		case ESocketMessageType::AddConversation:		return "add_conversation";
		case ESocketMessageType::InitialClientData:		return "initial_client_data";
		case ESocketMessageType::InitialConversations:	return "initial_conversations";
		case ESocketMessageType::Error:					return "error";

		case ESocketMessageType::Unknown:
		default:										return "unknown";
	}
}

FSocket::FSocket(int32 InSocketIndex, std::string InHost, const int32 InPort, bool bInUseSSL, const std::string& InKeyPath, const std::string& InCertPath)
	: SocketIndex(InSocketIndex)
	, Host(InHost)
	, Port(InPort)
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

void FSocket::AddDeferTaskForConnectionId(const Uint64 UserId, FFunctorLambda<void, void*>& FunctionToCallOnSocket)
{
	UserIdToWebSocketPtrMapMutex.lock_shared();
	std::optional<void*> WebSocketOptionalPtr = UserIdToWebSocketPtrMap.FindValueByKey(UserId);
	if (WebSocketOptionalPtr.has_value())
	{
		void* ws = WebSocketOptionalPtr.value();
		UserIdToWebSocketPtrMapMutex.unlock_shared();

		uWS::Loop* SocketLoop = SocketAppWrapper.GetLoop();
		SocketLoop->defer([FunctionToCallOnSocket, ws]() mutable {
			FunctionToCallOnSocket.operator()(ws);
		});
	}
	}

void FSocket::BeforeRunAsync()
{

}

template<bool SSL>
auto CreateSocketBehavior(FSocket* Socket) {
	return typename uWS::TemplatedApp<SSL>::template WebSocketBehavior<FWebSocketSessionData>{
		/* Settings */
		.compression = uWS::DISABLED,
		.maxPayloadLength = 16 * 1024,
		.idleTimeout = 60 * 5, // Time in seconds
		.maxBackpressure = 512 * 1024,
		.sendPingsAutomatically = true,
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
		.ping = [Socket](auto* ws, std::string_view message)
		{
			Socket->OnPing(ws);
		},
		.pong = [Socket](auto* ws, std::string_view message)
		{
			Socket->OnPong(ws);
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

	SocketAppWrapper.listen(Host, Port, [&](us_listen_socket_t* listenSocket)
	{
		if (listenSocket != nullptr)
		{
			AppListenSocket = listenSocket;

			LOG_INFO("Server, id: '" << SocketIndex << "', listening host: '" << Host << "', on port: '" << Port << "'.");
		}
		else
		{
			LOG_ERROR("Failed to listen on: " << Host << "@" << Port << ", id: '" << SocketIndex << "'.");
		}
	});

	BeforeRunAsync();

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
		UserIdToWebSocketPtrMapMutex.lock();
		UserIdToWebSocketPtrMap[WebSocketSessionData->UserId] = ws;
		UserIdToWebSocketPtrMapMutex.unlock();

		FUserManager* UserManger = ProjectEngine->GetUserManager();
		const Uint64 UserId = WebSocketSessionData->UserId;
		std::vector<std::shared_ptr<FUser>> OutUsers;
		UserManger->GetUsersByIds({ UserId }, OutUsers);

		// Add subscribe action
		{
			// SUBSCRIBE to a topic named after the user ID
			// syntax: "user_<id>"
			const std::string UserTopic = GenerateUserTopic(UserId);
			ws->subscribe(UserTopic);
		}

		// Add per user information about Socket
		if (!OutUsers.empty())
		{
			// OutUsers array keeps pointer in this case
			FUser* User = OutUsers[0].get();
			if (User != nullptr)
			{
				User->SetSocketId(SocketIndex);
				User->SetUserStatus(EUserStatus::Online);
			}
		}

		// Send initial client data
		{
			nlohmann::json JsonData;
			JsonData["user_id"] = UserId;

			if (!OutUsers.empty() && OutUsers[0].get() != nullptr)
			{
				JsonData["user_display_name"] = OutUsers[0]->GetDisplayedName();
			}
			else
			{
				LOG_ERROR("Missing User! Id = '" << UserId << "'.");
			}

			nlohmann::json JsonRoot;
			JsonRoot["type"] = SocketMessageTypeToString(ESocketMessageType::InitialClientData);
			JsonRoot["data"] = JsonData;

			// Send initial client data
			ws->send(JsonRoot.dump(), uWS::TEXT);
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

	FWebSocketSessionData* WebSocketSessionData = ws->getUserData();
	if (WebSocketSessionData != nullptr)
	{
		const Uint64 UserId = WebSocketSessionData->UserId;

		UserIdToWebSocketPtrMapMutex.lock();
		UserIdToWebSocketPtrMap.Remove(UserId);
		UserIdToWebSocketPtrMapMutex.unlock();

		FUserManager* UserManger = ProjectEngine->GetUserManager();
		std::vector<std::shared_ptr<FUser>> OutUsers;
		UserManger->GetUsersByIds({ UserId }, OutUsers);

		// Remove per user information about Socket
		if (!OutUsers.empty())
		{
			// OutUsers array keeps pointer in this case
			FUser* User = OutUsers[0].get();
			if (User != nullptr)
			{
				User->SetSocketId(-1);
				User->SetUserStatus(EUserStatus::Offline);
			}
		}
	}
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

void FSocket::OnPing(auto* ws)
{
#if DEBUG
	LOG_DEBUG("Received ping");
#endif
}

void FSocket::OnPong(auto* ws)
{
#if DEBUG
	LOG_DEBUG("Received pong");
#endif
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
				if (JsonMessage["data"].contains("conversation_id") && JsonMessage["data"].contains("content"))
				{
					const std::string ConversationIdString = JsonMessage["data"]["conversation_id"];
					const Uint64 ConversationId = std::stoull(ConversationIdString);
					const std::string Content = JsonMessage["data"]["content"];

					// Handle send message
					OnMessageReceived_Message(ws, opCode, ConversationId, Content);
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

			case ESocketMessageType::UserStatus:
			{
				if (JsonMessage["data"].contains("user_id"))
				{
					Uint64 UserId = JsonMessage["data"]["user_id"];

					// Handle mark read
					OnMessageReceived_UserStatus(ws, opCode, UserId);
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

			case ESocketMessageType::LoadMoreMessages:
			{
				FWebSocketSessionData* WebSocketSessionData = ws->getUserData();
				if (WebSocketSessionData != nullptr)
				{
					if (JsonMessage["data"].contains("conversation_id") && JsonMessage["data"].contains("offset") && JsonMessage["data"].contains("limit"))
					{
						const std::string ConversationIdString = JsonMessage["data"]["conversation_id"];
						const std::string OffsetString = JsonMessage["data"]["offset"];
						const std::string LimitString = JsonMessage["data"]["limit"];
						const Uint64 ConversationId = atoi(ConversationIdString.c_str());
						const int32 Offset = atoi(OffsetString.c_str());
						const int32 Limit = atoi(LimitString.c_str());

						OnMessageReceived_LoadMoreMessages(ws, opCode, ConversationId, WebSocketSessionData->UserId, Offset, Limit);
					}
				}

				break;
			}

			case ESocketMessageType::GetConversations:
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

						OnMessageReceived_GetConversations(ws, opCode, WebSocketSessionData->UserId, Offset, Limit);
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
		// Pong back
		ws->send(message, uWS::PONG);

		// Update status
		FWebSocketSessionData* WebSocketSessionData = ws->getUserData();
		if (WebSocketSessionData != nullptr)
		{
			const Uint64& ConnectionUserId = WebSocketSessionData->UserId;
			FUserManager* UserManager = ProjectEngine->GetUserManager();
			UserManager->UpdateUserActivity(ConnectionUserId);
		}
	}
}

void FSocket::OnMessageReceived_Pong(auto* ws, std::string_view message, uWS::OpCode opCode)
{
	// @TODO We should note that connections is not dead

	// Currently there is
	// SET ONLINE on connection
	// and
	// SET OFFLINE on disconnection
	/*
	if (opCode == uWS::PONG)
	{
		// Update status
		FWebSocketSessionData* WebSocketSessionData = ws->getUserData();
		if (WebSocketSessionData != nullptr)
		{
			const Uint64& ConnectionUserId = WebSocketSessionData->UserId;
			FUserManager* UserManager = ProjectEngine->GetUserManager();
			UserManager->UpdateUserActivity(ConnectionUserId);
		}
	}
	*/
}

void FSocket::OnMessageReceived_Message(auto* ws, uWS::OpCode opCode, const Uint64 ConversationId, const std::string& Content)
{
	FWebSocketSessionData* WebSocketSessionData = ws->getUserData();
	if (WebSocketSessionData != nullptr)
	{
		FConversationsManager* ConversationsManager = ProjectEngine->GetConversationsManager();
		FUserManager* UserManager = ProjectEngine->GetUserManager();
		FSocketManager* SocketManager = ProjectEngine->GetSocketManager();
		std::shared_ptr<FConversationData> Conversation = ConversationsManager->GetConversation(ConversationId);
		const Uint64& ConnectionUserId = WebSocketSessionData->UserId;
		if (Conversation != nullptr)
		{
			ConversationsManager->AddMessage(ConversationId, ConnectionUserId, Content);

			// Broadcast new message
			for (Uint64 UserId : Conversation->UsersIds)
			{
				nlohmann::json MessageJson;
				MessageJson["sender_id"] = ConnectionUserId;
				MessageJson["conversation_id"] = ConversationId;
				MessageJson["conversation_message"] = Content;

				nlohmann::json JsonRoot;
				JsonRoot["type"] = SocketMessageTypeToString(ESocketMessageType::Message);
				JsonRoot["message"] = MessageJson;

				if (UserId == ConnectionUserId)
				{
					// Publish does not work for self, so we need special case
					ws->send(JsonRoot.dump(), uWS::OpCode::TEXT);
				}
				else
				{
					std::vector<std::shared_ptr<FUser>> OutIds;
					UserManager->GetUsersByIds({ UserId }, OutIds);
					if (!OutIds.empty())
					{
						FUser* User = OutIds[0].get();

						FFunctorLambda<void, void*> SocketAccessFunctor = [this, JsonRoot, UserId](void* ws)
						{
							auto* WebSocket = static_cast<uWS::WebSocket<false, true, FUserSessionData>*>(ws);

							// To send message to a specific user
							const std::string UserTopic = GenerateUserTopic(UserId);
							if (WebSocket->isSubscribed(UserTopic))
							{
								const bool bSentPublish = WebSocket->send(JsonRoot.dump(), uWS::OpCode::TEXT);
									
#if DEBUG
								LOG_INFO("Backpressure: " << WebSocket->getBufferedAmount());
								LOG_INFO("Topic: '" << UserTopic << "', subscribed: '" << WebSocket->isSubscribed(UserTopic) << "'");
								LOG_INFO("Topic: '" << UserTopic << "', sent publish: '" << bSentPublish << "'");
#endif
							}
						};
						SocketManager->EnqueueTaskForUserAtSocket(User->GetSocketId(), UserId, SocketAccessFunctor);
					}
				}

				UserManager->UpdateUserActivity(ConnectionUserId);
			}
		}
	}
}

void FSocket::OnMessageReceived_Typing(auto* ws, uWS::OpCode opCode, const Uint64 ConversationId)
{
	FWebSocketSessionData* WebSocketSessionData = ws->getUserData();
	if (WebSocketSessionData != nullptr)
	{
		const Uint64& ConnectionUserId = WebSocketSessionData->UserId;
		FConversationsManager* ConversationsManager = ProjectEngine->GetConversationsManager();
		FSocketManager* SocketManager = ProjectEngine->GetSocketManager();
		FUserManager* UserManager = ProjectEngine->GetUserManager();
		std::shared_ptr<FConversationData> Conversation = ConversationsManager->GetConversation(ConversationId);
		std::vector<std::shared_ptr<FUser>> Users;
		UserManager->GetUsersByIds(Conversation->UsersIds.Vector, Users);

		nlohmann::json MessageJson;
		MessageJson["conversation_id"] = ConversationId;
		MessageJson["typing_id"] = ConnectionUserId;

		nlohmann::json JsonRoot;
		JsonRoot["type"] = SocketMessageTypeToString(ESocketMessageType::Typing);
		JsonRoot["message"] = MessageJson;

		for (std::shared_ptr<FUser>& User : Users)
		{
			if (User->GetUserId() != ConnectionUserId)
			{
				FFunctorLambda<void, void*> SocketAccessFunctor = [this, JsonRoot, User](void* ws)
				{
					auto* WebSocket = static_cast<uWS::WebSocket<false, true, FUserSessionData>*>(ws);

					// To send message to a specific user
					const std::string UserTopic = GenerateUserTopic(User->GetUserId());
					if (WebSocket->isSubscribed(UserTopic))
					{
						const bool bSentPublish = WebSocket->send(JsonRoot.dump(), uWS::OpCode::TEXT);

#if DEBUG
						LOG_INFO("Backpressure: " << WebSocket->getBufferedAmount());
						LOG_INFO("Topic: '" << UserTopic << "', subscribed: '" << WebSocket->isSubscribed(UserTopic) << "'");
						LOG_INFO("Topic: '" << UserTopic << "', sent publish: '" << bSentPublish << "'");
#endif
					}
				};
				SocketManager->EnqueueTaskForUserAtSocket(User->GetSocketId(), User->GetUserId(), SocketAccessFunctor);
			}
		}
	}
}

void FSocket::OnMessageReceived_MarkRead(auto* ws, uWS::OpCode opCode, const Uint64 ConversationId)
{
	FWebSocketSessionData* WebSocketSessionData = ws->getUserData();
	if (WebSocketSessionData != nullptr)
	{
		FConversationsManager* ConversationsManager = ProjectEngine->GetConversationsManager();
		const Uint64& ConnectionUserId = WebSocketSessionData->UserId;


	}
}

void FSocket::OnMessageReceived_UserStatus(auto* ws, uWS::OpCode opCode, Uint64 UserId)
{
	FUserManager* UserManager = ProjectEngine->GetUserManager();

	std::vector<std::shared_ptr<FUser>> UserPtrArray;
	if (UserManager->GetUsersByIds({ UserId }, UserPtrArray) && UserPtrArray.size() == 1)
	{
		std::shared_ptr<FUser> UserPtr = UserPtrArray[0];

		nlohmann::json MessageJson;
		MessageJson["user_id"] = UserPtr->GetUserId();
		MessageJson["status"] = UserStatusToString(UserPtr->GetUserStatus());

		nlohmann::json JsonRoot;
		JsonRoot["type"] = SocketMessageTypeToString(ESocketMessageType::UserStatus);;
		JsonRoot["message"] = MessageJson;

		// Publish does not work for self, so we need special case
		ws->send(JsonRoot.dump(), uWS::OpCode::TEXT);
	}
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

void FSocket::OnMessageReceived_LoadMoreMessages(auto* ws, uWS::OpCode opCode, Uint64 ConversationId, Uint64 CurrentUserId, int32 Offset, int32 Count)
{
	nlohmann::json JsonRoot;
	JsonRoot["type"] = SocketMessageTypeToString(ESocketMessageType::LoadMoreMessages);

	if (ConversationId > 0 && Offset > 0 && Count > 0)
	{
		FConversationsManager* ConversationsManager = ProjectEngine->GetConversationsManager();
		std::shared_ptr<FConversationData> Conversation = ConversationsManager->GetConversation(ConversationId);
		if (Conversation != nullptr && Conversation->UsersIds.Contains(CurrentUserId))
		{
			std::vector<FConversationMessageData> MessagesInRange = ConversationsManager->GetConversationMessagesForRange(Conversation, Offset, Count);
			if (MessagesInRange.size() > 0)
			{
				JsonRoot["status"] = "success";

				nlohmann::json MessagesJsonArray = nlohmann::json::array();

				for (FConversationMessageData& Message : MessagesInRange | std::views::reverse)
				{
					nlohmann::json NewMessage;
					NewMessage["message"] = Message.Message;
					NewMessage["sender_id"] = Message.SenderId;

					MessagesJsonArray.push_back(NewMessage);
				}

				JsonRoot["message"] = MessagesJsonArray;
			}
			else
			{
				JsonRoot["status"] = "no_more_messages";
				JsonRoot["message"] = "No messages in range.";
			}
		}
		else
		{
			JsonRoot["status"] = "unauthorized";
			JsonRoot["message"] = "Chat not suitable for user.";
		}
	}
	else
	{
		JsonRoot["status"] = "wrong_input";
		JsonRoot["message"] = "One of input parameters: ConversationId, Offset, Count is wrong.";
	}

	ws->send(JsonRoot.dump(), uWS::TEXT);
}

void FSocket::OnMessageReceived_GetConversations(auto* ws, uWS::OpCode opCode, Uint64 CurrentUserId, int32 Offset, int32 Limit)
{
	FConversationsManager* ConversationsManager = ProjectEngine->GetConversationsManager();

	CArray<Uint64> ConversationIds;
	ConversationsManager->GetLastConversationByUserId(CurrentUserId, Offset, Limit, ConversationIds);

	nlohmann::json ConversationsJsonArray = FormatConversationIntoJson(ConversationIds);

	nlohmann::json JsonRoot;
	JsonRoot["type"] = SocketMessageTypeToString(ESocketMessageType::GetConversations);
	JsonRoot["message"] = ConversationsJsonArray;

	// Send initial client data
	ws->send(JsonRoot.dump(), uWS::TEXT);
}

void FSocket::OnMessageReceived_AddConversation(auto* ws, uWS::OpCode opCode, const Uint64 OtherUserId)
{
	FWebSocketSessionData* WebSocketSessionData = ws->getUserData();
	if (WebSocketSessionData != nullptr && OtherUserId > 0)
	{
		CArray<Uint64> UserIdArray;
		UserIdArray.Push(WebSocketSessionData->UserId);
		UserIdArray.Push(OtherUserId);

		FConversationsManager* ConversationManager = ProjectEngine->GetConversationsManager();

		// Find or create conversation
		const Uint64 ConversationId = ConversationManager->GetOrCreateConversation(UserIdArray.Vector);

		const nlohmann::json MessageJson = FormatConversationIntoJson({ ConversationId });

		nlohmann::json ReturnJson;
		ReturnJson["type"] = SocketMessageTypeToString(ESocketMessageType::AddConversation);
		ReturnJson["message"] = MessageJson;
		ws->send(ReturnJson.dump(), opCode);
	}
}

std::string FSocket::GenerateUserTopic(const Uint64 UserId)
{
	return "user_" + std::to_string(UserId);
}

nlohmann::json FSocket::FormatConversationIntoJson(const CArray<Uint64>& ConversationIds)
{
	FConversationsManager* ConversationsManager = ProjectEngine->GetConversationsManager();
	FUserManager* UserManager = ProjectEngine->GetUserManager();

	nlohmann::json ConversationsJsonArray = nlohmann::json::array();

	for (Uint64 ConversationId : ConversationIds)
	{
		std::shared_ptr<FConversationData> Conversation = ConversationsManager->GetConversation(ConversationId);
		if (Conversation != nullptr)
		{
			nlohmann::json NewConversation;
			NewConversation["id"] = ConversationId;

			std::vector<std::shared_ptr<FUser>> OutUsers;
			UserManager->GetUsersByIds(Conversation->UsersIds.Vector, OutUsers);

			// Add users (id to name)
			{
				nlohmann::json UsersJsonArray = nlohmann::json::array();

				for (std::shared_ptr<FUser>& UserPtr : OutUsers)
				{
					{
						nlohmann::json NewUser;

						NewUser["id"] = UserPtr->GetUserId();
						NewUser["name"] = UserPtr->GetDisplayedName();
						NewUser["status"] = UserStatusToString(UserPtr->GetUserStatus());

						UsersJsonArray.push_back(NewUser);
					}
				}

				NewConversation["users"] = UsersJsonArray;
			}

			// Add users Ids
			NewConversation["userids"] = Conversation->UsersIds.Vector;

			// Add messages
			{
				std::vector<FConversationMessageData> LastMessages = Conversation->MessagesMap.PeekFirst(25);
				nlohmann::json MessagesJsonArray = nlohmann::json::array();

				for (FConversationMessageData& Message : LastMessages | std::views::reverse)
				{
					nlohmann::json NewMessage;
					NewMessage["message"] = Message.Message;
					NewMessage["sender_id"] = Message.SenderId;

					MessagesJsonArray.push_back(NewMessage);
				}

				NewConversation["messages"] = MessagesJsonArray;
			}

			ConversationsJsonArray.push_back(NewConversation);
		}
	}

	return ConversationsJsonArray;
}

nlohmann::json FSocket::FormatUsersToJson(const std::vector<uint64_t>& UserIds, const std::vector<std::string>& DisplayNames)
{
	nlohmann::json DataUserArray = nlohmann::json::array();

	for (size_t i = 0; i < UserIds.size(); i++)
	{
		nlohmann::json SingleUser = {
			{"id", UserIds[i]},
			{"displayName", DisplayNames[i]}
		};

		DataUserArray.push_back(SingleUser);
	}

	// Standard JSON with root object
	nlohmann::json JsonRoot;
	JsonRoot["data"] = DataUserArray;
	return JsonRoot;
}
