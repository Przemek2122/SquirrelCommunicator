#pragma once

#include <nlohmann/json_fwd.hpp>

#include "Misc/WebSockets/AppWrapper.h"
#include "soci/session.h"

struct FConversationData;
class FProjectEngine;

/** Enum for each message sent using socket */
enum class ESocketMessageType : uint8
{
	Unknown,

	Message,
	Typing,
	MarkRead,
	SearchUser,
	GetConversation,
	AddConversation,
	InitialClientData,
	InitialConversations,
	Error
};

ESocketMessageType StringToSocketMessageType(const std::string& InTypeString);
std::string SocketMessageTypeToString(ESocketMessageType InTypeEnum);

struct FConversationInfo
{
	long long ConversationId;
	std::string LastMessageAt;
	long long LastReadMessageId;
};

/**
 * Single web socket
 * Currently also integrates as wrapper for uWebSockets template
 */
class FSocket
{
public:
	FSocket(int32 InPort, bool bInUseSSL, const std::string& InKeyPath, const std::string& InCertPath);
	~FSocket();

	void Async();

	/** uWS lambdas extensions */
	void OnClientConnected(auto* ws);
	void OnClientDisconnected(auto* ws, int code, std::string_view message);
	void OnMessageReceived(auto* ws, std::string_view message, uWS::OpCode opCode);

private:
	/** Default uWS OpCodes */
	void OnMessageReceived_TEXT(auto* ws, std::string_view message, uWS::OpCode opCode);
	void OnMessageReceived_Ping(auto* ws, std::string_view message, uWS::OpCode opCode);
	void OnMessageReceived_Pong(auto* ws, std::string_view message, uWS::OpCode opCode);

	/** Custom enums */
	void OnMessageReceived_SendMessage(auto* ws, uWS::OpCode opCode, Uint64 ReceiverId, const std::string& Content);
	void OnMessageReceived_Typing(auto* ws, uWS::OpCode opCode, Uint64 ConversationId);
	void OnMessageReceived_MarkRead(auto* ws, uWS::OpCode opCode, Uint64 ConversationId);
	void OnMessageReceived_SearchUser(auto* ws, uWS::OpCode opCode, const std::string& Pattern);
	void OnMessageReceived_GetConversation(auto* ws, uWS::OpCode opCode, Uint64 CurrentUserId, int32 Offset, int32 Limit);
	void OnMessageReceived_AddConversation(auto* ws, uWS::OpCode opCode, Uint64 OtherUserId);

	std::string GenerateUserTopic(Uint64 UserId);
	nlohmann::json FormatConversationIntoJson(std::shared_ptr<FConversationData>& ConversationDataPtr);
	nlohmann::json FormatUsersToJson(const std::vector<uint64_t>& UserIds, const std::vector<std::string>& DisplayNames);
	Uint64 AddConversation(soci::session& DataBaseSession, const std::vector<Uint64>& UserIds);
	Uint64 FindDirectConversation(soci::session& Sql, Uint64 User1Id, Uint64 User2Id);
	std::vector<FConversationInfo> GetConversationsFromRange(soci::session& Sql, Uint64 UserId, int32 Offset, int32 Limit);

private:
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

};
