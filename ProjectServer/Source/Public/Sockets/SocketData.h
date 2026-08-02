// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

#include "EngineCompat.h"
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

    /** Servers section (handles room/server creation, channel ops, invites, voice, etc.) */
    Servers,

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
    MessageEdit,
    Typing,
    MessageDelivered,
    MessageRead,
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

/** Enum for each message sent in servers sections in socket */
enum class ESocketMessageServersType : uint8
{
    Unknown = 0,

    // Client -> Server (requests/actions)
    CreateRoom,
    JoinRoom,
    LeaveRoom,
    RoomMessage,
    CreateChannel,
    MoveChannel,            // Request to reorder a channel (change position)
    ReorderChannels,        // Request to reorder all channels at once (drag-and-drop batch)
    DeleteChannel,          // Request to delete a channel
    RenameChannel,          // Request to rename a channel
    ServerInvite,
    RoomJoinVoice,
    RoomLeaveVoice,
    GetServerList,          // Request list of servers user belongs to (supports offset/limit pagination)
    GetServerMessages,      // Request message history for a channel
    ServerCreateInvite,     // Request to generate an invite code
    ServerJoinInvite,       // Request to join via invite code
    ServerUpdateMemberPermissions, // Request to update a member's permissions
    ServerDeleteInvite,     // Request to delete an invite by code
    ServerListInvites,      // Request to list invites with pagination

    // Server -> Client (responses/events)
    RoomCreated,
    RoomUserJoined,
    RoomUserLeft,
    RoomChannelCreated,
    RoomChannelMoved,       // Channel was reordered (broadcast + response)
    RoomChannelsReordered,  // All channels were reordered at once (broadcast + response, drag-and-drop)
    RoomChannelDeleted,     // Channel was deleted (broadcast + response)
    RoomChannelRenamed,     // Channel was renamed (broadcast + response)
    RoomUserVoiceJoin,
    RoomUserVoiceLeave,
    RoomMemberStatus,
    ServerList,             // Response with servers data
    ServerMessages,         // Response with message history
    ServerInviteCreated,    // Response with generated invite code
    ServerJoined,           // Response after joining via invite (full room data)
    ServerMemberPermissionsUpdated, // Response after permissions update
    ServerInviteDeleted,    // Response after deleting an invite
    ServerInvitesList,      // Response with paginated invite list

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

/** Converts incoming string types to the Servers-specific enum. Uses O(1) compile-time hashing. */
ESocketMessageServersType StringToSocketMessageServersType(std::string_view InTypeString);

/** Converts Servers-specific enum back to string for outgoing messages. */
std::string SocketMessageServersTypeToString(ESocketMessageServersType InTypeEnum);
