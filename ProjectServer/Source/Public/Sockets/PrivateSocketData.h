// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

#include <uwebsockets/WebSocketProtocol.h>
#include "SocketData.h"
#include "nlohmann/json_fwd.hpp"

class FProjectEngine;
class FSocket;

/**
 * Helper class for Socket class.
 * Only stores conversation functions
 */
class FPrivateSocketData
{
public:
    explicit FPrivateSocketData(FSocket* InSocket);

    /** Function for jumping into all other functions in this class */
    void PrimarySwitch(AnyWebSocket wsVariant, nlohmann::json& JsonMessage, uWS::OpCode opCode);

    /** Called when user is sending a message */
    void OnMessageReceived_Message(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ConversationId, const std::string& Content);

    /** Called when user is typing */
    void OnMessageReceived_Typing(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ConversationId);

    /** Called when user is reading message */
    void OnMessageReceived_MarkRead(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ConversationId);

    /** @TODO: Does not work properly */
    void OnMessageReceived_UserStatus(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 UserId);

    /** Called when user is searching for another user */
    void OnMessageReceived_SearchUser(AnyWebSocket wsVariant, uWS::OpCode opCode, const std::string& Pattern);

    /** Used by frontend when user wants more messages, Offset and Limit are used to define if we want conversations 0-5, 5-10, etc... */
    void OnMessageReceived_LoadMoreMessages(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ConversationId, Uint64 CurrentUserId, int32 Offset, int32 Count);

    /** Used to get conversations list with offset and limit */
    void OnMessageReceived_GetConversations(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 CurrentUserId, int32 Offset, int32 Limit);

    /** Used to create a new conversation */
    void OnMessageReceived_AddConversation(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 OtherUserId);

    /** Used to create friend request */
    void OnMessageReceived_CreateFriendRequest(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 OtherUserId);

    /** Used to add friend */
    void OnMessageReceived_AcceptFriendRequest(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 OtherUserId);

    /** Used to reject friend request */
    void OnMessageReceived_RejectFriendRequest(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 OtherUserId);

    /** Used to cancel friend request */
    void OnMessageReceived_CancelFriendRequest(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 OtherUserId);

    /** Used to remove friend */
    void OnMessageReceived_RemoveFriend(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 OtherUserId);

    /** Used to get friend requests list */
    void OnMessageReceived_GetFriendRequestsList(AnyWebSocket wsVariant, uWS::OpCode opCode, int32 Offset, int32 Limit);

    /** Used to get friend list */
    void OnMessageReceived_GetFriendList(AnyWebSocket wsVariant, uWS::OpCode opCode, int32 Offset, int32 Limit);

private:
    /** returns conversation json aray */
    nlohmann::json FormatConversationIntoJson(const CArray<Uint64>& ConversationIds);
    nlohmann::json FormatUsersToJson(const std::vector<uint64_t>& UserIds, const std::vector<std::string>& DisplayNames);

    nlohmann::json FormatDataToJson(const ESocketMessagePrivateType Type, const std::string& Message);

private:
    /** Pointer to main class */
    FSocket* Socket;

    /** Engine pointer */
    FProjectEngine* ProjectEngine;

};
