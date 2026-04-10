#include "Sockets/Socket.h"

#include "ProjectEngine.h"
#include "AbuseProtection/AbuseProtection.h"
#include "Auth/UserManager.h"
#include "DataBase/DataBaseConnect.h"
#include "Misc/WebSockets/CookieHelper.h"
#include "Sockets/WebSocketSessionData.h"
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <utility>

FSocket::FSocket(const int32 InSocketIndex, std::string InHost, const int32 InPort, bool bInUseSSL, const std::string& InKeyPath, const std::string& InCertPath)
	: SocketIndex(InSocketIndex)
	, Host(std::move(InHost))
	, Port(InPort)
	, bUseSSL(bInUseSSL)
	, SocketAppWrapper(bInUseSSL, InKeyPath, InCertPath)
	, AppListenSocket(nullptr)
	, ProjectEngine(dynamic_cast<FProjectEngine*>(FGlobalDefines::GEngine))
	, SocketMiscData(this)
	, SocketRoomsData(this)
{
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
	std::shared_lock<std::shared_mutex> Lock(UserIdToWebSocketPtrMapMutex);
	const std::optional<void*> WebSocketOptionalPtr = UserIdToWebSocketPtrMap.FindValueByKey(UserId);
	if (WebSocketOptionalPtr.has_value())
	{
		void* ws = WebSocketOptionalPtr.value();

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
		{
			std::unique_lock<std::shared_mutex> Lock(UserIdToWebSocketPtrMapMutex);
			UserIdToWebSocketPtrMap[WebSocketSessionData->UserId] = ws;
		}

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
				JsonData["user_display_name"] = OutUsers[0]->GetUserNameString();
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

		{
			std::unique_lock<std::shared_mutex> Lock(UserIdToWebSocketPtrMapMutex);
			UserIdToWebSocketPtrMap.Remove(UserId);
		}

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

std::string FSocket::GenerateUserTopic(const Uint64 UserId)
{
	return "user_" + std::to_string(UserId);
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
					SocketMiscData.OnMessageReceived_Message(ws, opCode, ConversationId, Content);
				}

				break;
			}

			case ESocketMessageType::Typing:
			{
				if (JsonMessage["data"].contains("conversationId"))
				{
					const Uint64 ConversationId = JsonMessage["data"]["conversationId"];

					// Handle typing
					SocketMiscData.OnMessageReceived_Typing(ws, opCode, ConversationId);
				}

				break;
			}

			case ESocketMessageType::MarkRead:
			{
				if (JsonMessage["data"].contains("conversationId"))
				{
					Uint64 ConversationId = JsonMessage["data"]["conversationId"];

					// Handle mark read
					SocketMiscData.OnMessageReceived_MarkRead(ws, opCode, ConversationId);
				}

				break;
			}

			case ESocketMessageType::UserStatus:
			{
				if (JsonMessage["data"].contains("user_id"))
				{
					Uint64 UserId = JsonMessage["data"]["user_id"];

					// Handle mark read
					SocketMiscData.OnMessageReceived_UserStatus(ws, opCode, UserId);
				}

				break;
			}

			case ESocketMessageType::SearchUser:
			{
				if (JsonMessage["data"].contains("search_target"))
				{
					const std::string Pattern = JsonMessage["data"]["search_target"];

					SocketMiscData.OnMessageReceived_SearchUser(ws, opCode, Pattern);
				}

				break;
			}

			case ESocketMessageType::RequestAddUser:
			{
				FWebSocketSessionData* WebSocketSessionData = ws->getUserData();
				if (WebSocketSessionData != nullptr)
				{
					if (JsonMessage["data"].contains("user_id"))
					{
						const std::string OtherUserIdAsString = JsonMessage["data"]["user_id"];
						const Uint64 OtherUserId = atoi(OtherUserIdAsString.c_str());

						SocketMiscData.OnMessageReceived_RequestAddUser(ws, opCode, WebSocketSessionData->UserId, OtherUserId);
					}
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

						SocketMiscData.OnMessageReceived_LoadMoreMessages(ws, opCode, ConversationId, WebSocketSessionData->UserId, Offset, Limit);
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

						SocketMiscData.OnMessageReceived_GetConversations(ws, opCode, WebSocketSessionData->UserId, Offset, Limit);
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

					SocketMiscData.OnMessageReceived_AddConversation(ws, opCode, OtherUserId);
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
