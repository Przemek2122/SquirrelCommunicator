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
    void CreateServer(AnyWebSocket wsVariant, uWS::OpCode opCode, const std::string& ServerName);
    void JoinServer(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ServerId);
    void LeaveServer(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ServerId);
    void ServerMessage(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ServerId, Uint64 ChannelId, const std::string& Content, EMessageType MessageType = EMessageType::Text);
    void CreateChannel(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ServerId, const std::string& ChannelName, const std::string& ChannelType);
    void ServerInvite(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ServerId, Uint64 TargetUserId);
    void ServerJoinVoice(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ServerId, Uint64 ChannelId);
    void ServerLeaveVoice(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ServerId, Uint64 ChannelId);
    void HandleGetVoiceChannelUsers(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ServerId, Uint64 ChannelId);

    // New WebSocket-based handlers (replacing REST endpoints)
    void HandleGetServerList(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint32 Offset = 0, Uint32 Limit = 50);
    void HandleGetServerMessages(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ServerId, Uint64 ChannelId, Uint64 Before, Uint32 Limit);

    /**
     * Create an invite code for a server.
     *  - MaxUses: maximum number of times the invite can be used (0 = use backend default)
     *  - ExpiresInSeconds: invitation lifetime (0 = use backend default, capped at 12 months)
     *  - Requires CAN_CREATE_INVITES permission (or server owner).
     */
    void HandleServerCreateInvite(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                  Uint64 ServerId, Uint32 MaxUses, Uint32 ExpiresInSeconds);

    void HandleServerJoinInvite(AnyWebSocket wsVariant, uWS::OpCode opCode, const std::string& InviteCode);

    /**
     * Update a member's permissions in a server.
     * Only the server owner (or members with CAN_MANAGE_PERMISSIONS in future) can do this.
     */
    void HandleUpdateMemberPermissions(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                       Uint64 ServerId, Uint64 TargetUserId, Uint64 NewPermissions);

    /**
     * Kick a member from a server.
     * Requires CAN_KICK_MEMBERS permission (or server owner).
     * The owner cannot be kicked. Users cannot kick themselves.
     */
    void HandleKickMember(AnyWebSocket wsVariant, uWS::OpCode opCode,
                          Uint64 ServerId, Uint64 TargetUserId);

    /**
     * Delete an invite by its code.
     * Requires CAN_CREATE_INVITES permission (or server owner).
     */
    void HandleServerDeleteInvite(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                  Uint64 ServerId, const std::string& InviteCode);

    /**
     * List invites for a server with pagination.
     * Requires CAN_CREATE_INVITES permission (or server owner).
     * Returns invites sorted by creation time (newest first) with invite_code, max_uses,
     * current_uses, remaining_uses, created_at, and expires_at.
     */
    void HandleServerListInvites(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                 Uint64 ServerId, Uint32 Start, Uint32 Count);

    /**
     * Move a single channel to a new position in the channel list.
     * Requires CAN_MANAGE_CHANNELS permission (or server owner).
     * All channels are renumbered after the move to eliminate gaps.
     */
    void HandleMoveChannel(AnyWebSocket wsVariant, uWS::OpCode opCode,
                           Uint64 ServerId, Uint64 ChannelId, int32 NewPosition);

    /**
     * Reorder all channels at once using a complete ordered array of channel IDs.
     * Designed for drag-and-drop UIs: the frontend sends the final desired order
     * and the server atomically updates all positions in a single DB transaction.
     * Requires CAN_MANAGE_CHANNELS permission (or server owner).
     *
     * The channel_ids array must contain every channel in the server exactly once.
     * Missing channels, duplicate IDs, or extra IDs are rejected.
     */
    void HandleReorderChannels(AnyWebSocket wsVariant, uWS::OpCode opCode,
                               Uint64 ServerId, const std::vector<Uint64>& ChannelIds);

    /**
     * Delete a channel from a server.
     * Requires CAN_MANAGE_CHANNELS permission (or server owner).
     * Deletes the channel and all its messages permanently.
     */
    void HandleDeleteChannel(AnyWebSocket wsVariant, uWS::OpCode opCode,
                             Uint64 ServerId, Uint64 ChannelId);

    /**
     * Rename a channel in a server.
     * Requires CAN_MANAGE_CHANNELS permission (or server owner).
     * Updates the channel name in DB and in-memory cache, then broadcasts server_channel_renamed.
     */
    void HandleRenameChannel(AnyWebSocket wsVariant, uWS::OpCode opCode,
                             Uint64 ServerId, Uint64 ChannelId, const std::string& NewName);

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

    /** Helper: build a server data JSON for frontend consumption */
    nlohmann::json BuildServerDataJson(Uint64 ServerId);

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
