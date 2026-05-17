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

enum class ESocketMessageSection : uint8
{
    Unknown = 0,

    /** Private message section */
    Priv,

    /** Rooms section */
    Rooms,

    Error = 255
};

/**
 * Enum for each message sent in private sections in socket
 * As most performance important enum, even with compile time hashing should be sorted into more recently used being on top
 */
enum class ESocketMessagePrivateType : uint8
{
    Unknown = 0,

    Message,
    Typing,
    MarkRead,
    UserStatus,
    SearchUser,
    LoadMoreMessages,
    GetConversations,
    AddConversation,
    GetFriendRequestList,
    GetFriendList,
    InitialClientData,
    InitialConversations,
    CreateFriendRequest,
    AcceptFriendRequest,
    RejectFriendRequest,
    CancelFriendRequest,
    RemoveFriend,
    DataStreamChannel,
    UserCalling,

    Error = 255
};

/** Enum for each message sent in rooms sections in socket */
enum class ESocketMessageRoomsType : uint8
{
    Unknown = 0,

    CreateRoom,

    Error = 255
};

/** Optimized String to Section conversion using compile-time hashing. Clean, readable, and O(1) performance. */
ESocketMessageSection StringToSocketMessageSection(std::string_view InTypeString);

/** Standard Enum to String conversion. The compiler optimizes this switch into a jump table (O(1)). */
std::string SocketMessageSectionToString(ESocketMessageSection InTypeEnum);

/** Optimized String to Section conversion using compile-time hashing. Clean, readable, and O(1) performance. */
ESocketMessagePrivateType StringToSocketMessagePrivateType(std::string_view InTypeString);

/** Standard Enum to String conversion. The compiler optimizes this switch into a jump table (O(1)). */
std::string SocketMessagePrivateTypeToString(ESocketMessagePrivateType InTypeEnum);

/** Converts incoming string types to the Rooms-specific enum. Uses O(1) compile-time hashing. */
ESocketMessageRoomsType StringToSocketMessageRoomsType(std::string_view InTypeString);

/** Converts Rooms-specific enum back to string for outgoing messages. */
std::string SocketMessageRoomsTypeToString(ESocketMessageRoomsType InTypeEnum);
