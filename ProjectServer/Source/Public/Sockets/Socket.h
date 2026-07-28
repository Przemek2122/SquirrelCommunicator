// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

#include "EngineCompat.h"

#include <shared_mutex>
#include <nlohmann/json_fwd.hpp>
#include "PrivateSocketData.h"
#include "ServersSocketData.h"
#include "Auth/UserManager.h"
#include "WebSocket/AppWrapper.h"

class FUser;
struct FConversationData;
class FProjectEngine;

/**
 * Single web socket
 * Currently also integrates as wrapper for uWebSockets template
 */
class FSocket
{
public:
	FSocket(int32 InSocketIndex, std::string InHost, int32 InPort, bool bInUseSSL, const std::string& InKeyPath, const std::string& InCertPath);
	~FSocket();

	/** Add task to be executed on this socket */
	void AddDeferTaskForConnectionId(Uint64 UserId, FFunctorLambda<void, void* /* ws */>& FunctionToCallOnSocket);

	/** Function called before entering socket loop */
	void BeforeRunAsync();

	/** Called from thread */
	void Async();

	/** uWS lambdas extensions */
	void OnClientConnected(auto* ws);
	void OnClientDisconnected(auto* ws, int code, std::string_view message);
	void OnMessageReceived(auto* ws, std::string_view message, uWS::OpCode opCode);
	void OnPing(auto* ws);
	void OnPong(auto* ws);

	static void EarlyExit(AnyWebSocket wsVariant, const char* Message, uWS::OpCode opCode);
	static std::string GenerateUserTopic(Uint64 UserId);
	static std::string GenerateVoiceRoomNameFromIds(std::vector<Uint64> IdArray);

	FProjectEngine* GetProjectEngine() const { return ProjectEngine; }

private:
	/** Begin Default uWS OpCodes */
	void OnMessageReceived_TEXT(auto* ws, std::string_view message, uWS::OpCode opCode);
	void OnMessageReceived_Ping(auto* ws, std::string_view message, uWS::OpCode opCode);
	void OnMessageReceived_Pong(auto* ws, std::string_view message, uWS::OpCode opCode);
	/** EndDefault uWS OpCodes */

	void AddWebSocketForConnectedUser(auto* ws, FWebSocketSessionData* WebSocketSessionData);
	void RemoveWebSocketForDisconnectedUser(Uint64 UserId);

	void BroadcastUserStatus(FUserManager* UserManger, Uint64 ConnectedUserId, EUserStatus NewUserStatus);

	void BroadcastToConversation(AnyWebSocket SenderWsVariant, const std::shared_ptr<FConversationData>& Conversation,
		Uint64 SenderUserId, const std::string& SerializedPayload);

private:
	/** per socket index to find which socket is user connected to */
	int32 SocketIndex;

	/** Socket listen host */
	std::string Host;

	/** Socket port */
	int32 Port;

	/** Is socket using SSL? */
	bool bUseSSL;

	/** Wrapper for template heavy uWebSocket socket */
	FSocketAppWrapper SocketAppWrapper;

	/** pointer from opening socket, used to close a socket if needed */
	us_listen_socket_t* AppListenSocket;

	/** Engine pointer */
	FProjectEngine* ProjectEngine;

	/** User id to socket ptr */
	CUnorderedMap<Uint64, AnyWebSocket> UserIdToWebSocketPtrMap;

	/** Mutex for UserIdToWebSocketPtrMap */
	std::shared_mutex UserIdToWebSocketPtrMapMutex;

	/** Represents miscellaneous data associated with a socket, created for readability due to big main class size. */
	FPrivateSocketData PrivateSocketData;

	/** Represents data related to servers, such as server management and user server associations, created for readability due to big main class size. */
	FServersSocketData ServersSocketData;
};
