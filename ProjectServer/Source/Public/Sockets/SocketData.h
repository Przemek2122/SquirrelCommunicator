// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

#include <variant>
#include <uwebsockets/WebSocket.h>

struct FWebSocketSessionData;

/**
 * Automatic template for UWebSockets
 * Probably not the most elegant solution but safe
 */
using AnyWebSocket = std::variant<
    uWS::WebSocket<false, true, FWebSocketSessionData>*,
    uWS::WebSocket<true, true, FWebSocketSessionData>*
>;

/** Enum for each message sent using a socket */
enum class ESocketMessageType : uint8
{
    Unknown,

    Message,
    Typing,
    MarkRead,
    UserStatus,
    SearchUser,
    RequestAddUser,
    LoadMoreMessages,
    GetConversations,
    AddConversation,
    InitialClientData,
    InitialConversations,
    Error
};

/** ESocketMessageType enum conversion from string */
ESocketMessageType StringToSocketMessageType(const std::string& InTypeString);

/** ESocketMessageType enum conversion to string */
std::string SocketMessageTypeToString(ESocketMessageType InTypeEnum);