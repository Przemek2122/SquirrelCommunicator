// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

#include "SocketData.h"
#include "nlohmann/json_fwd.hpp"

enum class EUserStatus : Uint8;
class FProjectEngine;
class FSocket;

class FServersSocketData
{
public:
    explicit FServersSocketData(FSocket* InSocket);

    /** Function for jumping into all other functions in this class */
    void PrimarySwitch(AnyWebSocket wsVariant, nlohmann::json& JsonMessage, uWS::OpCode opCode);

    // Client -> Server handlers
    void CreateRoom(AnyWebSocket wsVariant, uWS::OpCode opCode, const std::string& RoomName);
    void JoinRoom(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 RoomId);
    void LeaveRoom(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 RoomId);
    void RoomMessage(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 RoomId, Uint64 ChannelId, const std::string& Content);
    void CreateChannel(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 RoomId, const std::string& ChannelName, const std::string& ChannelType);
    void ServerInvite(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 RoomId, Uint64 TargetUserId);
    void RoomJoinVoice(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 RoomId, Uint64 ChannelId);
    void RoomLeaveVoice(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 RoomId, Uint64 ChannelId);

    // New WebSocket-based handlers (replacing REST endpoints)
    void HandleGetServerList(AnyWebSocket wsVariant, uWS::OpCode opCode);
    void HandleGetServerMessages(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 RoomId, Uint64 ChannelId, Uint64 Before, Uint32 Limit);

    /**
     * Create an invite code for a server.
     *  - MaxUses: maximum number of times the invite can be used (0 = use backend default)
     *  - ExpiresInSeconds: invitation lifetime (0 = use backend default, capped at 12 months)
     *  - Requires CAN_CREATE_INVITES permission (or server owner).
     */
    void HandleServerCreateInvite(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                  Uint64 RoomId, Uint32 MaxUses, Uint32 ExpiresInSeconds);

    void HandleServerJoinInvite(AnyWebSocket wsVariant, uWS::OpCode opCode, const std::string& InviteCode);

    /**
     * Update a member's permissions in a server.
     * Only the server owner (or members with CAN_MANAGE_PERMISSIONS in future) can do this.
     */
    void HandleUpdateMemberPermissions(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                       Uint64 RoomId, Uint64 TargetUserId, Uint64 NewPermissions);

    /**
     * Delete an invite by its code.
     * Requires CAN_CREATE_INVITES permission (or server owner).
     */
    void HandleServerDeleteInvite(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                  Uint64 RoomId, const std::string& InviteCode);

    /**
     * List invites for a server with pagination.
     * Requires CAN_CREATE_INVITES permission (or server owner).
     * Returns invites sorted by creation time (newest first) with invite_code, max_uses,
     * current_uses, remaining_uses, created_at, and expires_at.
     */
    void HandleServerListInvites(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                 Uint64 RoomId, Uint32 Start, Uint32 Count);

    /** Broadcast a member's status change to all servers they belong to */
    void BroadcastMemberStatus(Uint64 UserId, const std::string& UserName, EUserStatus NewStatus);

    /**
     * Called when a user disconnects from WebSocket.
     * Removes them from all voice channels and broadcasts server_voice_left to other members.
     */
    void CleanupUserVoiceChannels(Uint64 UserId, const std::string& UserName);

private:
    /** Helper: broadcast a JSON message to all members of a server */
    void BroadcastToServerMembers(Uint64 ServerId, const nlohmann::json& JsonMessage, Uint64 ExcludeUserId = 0);

    /** Helper: send a JSON message to a specific user */
    void SendToUser(Uint64 UserId, const nlohmann::json& JsonMessage);

    /** Helper: get the UserId from a WebSocket session */
    static Uint64 GetUserIdFromWS(AnyWebSocket wsVariant);

    /** Helper: get the UserName from engine by UserId */
    std::string GetUserName(Uint64 UserId);

    /** Helper: build a room data JSON for frontend consumption */
    nlohmann::json BuildRoomDataJson(Uint64 ServerId);

    /** Helper: get remote IP for rate limiting */
    std::string GetRemoteIP(AnyWebSocket wsVariant) const;

    /** Helper: check if the requesting user is the server owner */
    bool IsServerOwner(Uint64 ServerId, Uint64 UserId);

private:
    /** Pointer to main class */
    FSocket* Socket;

    /** Engine pointer */
    FProjectEngine* ProjectEngine;

    /** Key for generating tokens */
    std::string EncryptionKey;
};
