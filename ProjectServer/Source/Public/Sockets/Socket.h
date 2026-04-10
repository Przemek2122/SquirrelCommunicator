// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

#include <shared_mutex>
#include <nlohmann/json_fwd.hpp>
#include "SocketMiscData.h"
#include "SocketRoomsData.h"
#include "Misc/WebSockets/AppWrapper.h"

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

	static std::string GenerateUserTopic(Uint64 UserId);

	FProjectEngine* GetProjectEngine() const { return ProjectEngine; }

private:
	/** Begin Default uWS OpCodes */
	void OnMessageReceived_TEXT(auto* ws, std::string_view message, uWS::OpCode opCode);
	void OnMessageReceived_Ping(auto* ws, std::string_view message, uWS::OpCode opCode);
	void OnMessageReceived_Pong(auto* ws, std::string_view message, uWS::OpCode opCode);
	/** EndDefault uWS OpCodes */

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
	CUnorderedMap<Uint64, void*> UserIdToWebSocketPtrMap;

	/** Mutex for UserIdToWebSocketPtrMap */
	std::shared_mutex UserIdToWebSocketPtrMapMutex;

	/** Represents miscellaneous data associated with a socket, created for readability due to big main class size. */
	FSocketMiscData SocketMiscData;

	/** Represents data related to socket rooms, such as room management and user room associations, created for readability due to big main class size. */
	FSocketRoomsData SocketRoomsData;
};
