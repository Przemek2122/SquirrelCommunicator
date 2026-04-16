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

    /** Used to send request to add user as friend */
    void OnMessageReceived_RequestAddUser(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 CurrentUserId, Uint64 OtherUserId);

    /** Used by frontend when user wants more messages, Offset and Limit are used to define if we want conversations 0-5, 5-10, etc... */
    void OnMessageReceived_LoadMoreMessages(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ConversationId, Uint64 CurrentUserId, int32 Offset, int32 Count);

    /** Used to get conversations list with offset and limit */
    void OnMessageReceived_GetConversations(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 CurrentUserId, int32 Offset, int32 Limit);

    /** Used to create a new conversation */
    void OnMessageReceived_AddConversation(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 OtherUserId);

private:
    /** returns conversation json aray */
    nlohmann::json FormatConversationIntoJson(const CArray<Uint64>& ConversationIds);
    nlohmann::json FormatUsersToJson(const std::vector<uint64_t>& UserIds, const std::vector<std::string>& DisplayNames);

private:
    /** Pointer to main class */
    FSocket* Socket;

    /** Engine pointer */
    FProjectEngine* ProjectEngine;

};
