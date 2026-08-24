// Created by https://www.linkedin.com/in/przemek2122/ 2020-2026

#pragma once

#include "EngineCompat.h"
#include "Managers/Server.h"

#include <shared_mutex>
#include <unordered_map>
#include <vector>

class FServer;

/**
 * Result of a DeleteMessage operation.
 * Lets the socket layer translate internal failures into specific client errors.
 */
enum class EDeleteServerMessageResult : uint8
{
    Success,
    ServerNotFound,
    MessageNotFound,
    NotAuthorized,
    Failed
};

/**
 * Manager for user servers .
 * Handles:
 *  - Server creation and deletion
 *  - Server membership (join/leave/invite)
 *  - Channel CRUD within servers (including reordering via MoveChannel, batch reorder via ReorderChannels, and renaming via RenameChannel)
 *  - Messages within text channels
 *  - Invite code generation, resolution, listing, and deletion (with abuse protection)
 *  - Member permission management (bitfield)
 *  - Database persistence for all server data
 */
class FServersManager
{
public:
    FServersManager();

    /** Called every second by the engine for periodic tasks (abuse cleanup, etc.) */
    void PostSecondTick();

    /** Search for server with provided Id, will return nullptr if not found */
    std::shared_ptr<FServer> GetServerById(Uint64 InServerId);

    /** Get all servers a user is a member of */
    std::vector<std::shared_ptr<FServer>> GetUserServers(Uint64 UserId);

    /** Get just the server IDs a user belongs to (lightweight, no full server load) */
    std::vector<Uint64> GetUserServerIds(Uint64 UserId);

    /** Add server and returns Id of new server */
    Uint64 AddServer(const std::string& InServerName, Uint64 OwnerId);

    /** Remove server with provided Id */
    bool RemoveServer(Uint64 InServerId);

    /** Membership operations */
    bool AddUserToServer(Uint64 ServerId, Uint64 UserId, const std::string& UserName,
                         Uint64 Permissions = 0);
    bool RemoveUserFromServer(Uint64 ServerId, Uint64 UserId);
    bool IsUserInServer(Uint64 ServerId, Uint64 UserId);

    /** Permission management */
    bool UpdateMemberPermissions(Uint64 ServerId, Uint64 TargetUserId, Uint64 NewPermissions);
    Uint64 GetMemberPermissions(Uint64 ServerId, Uint64 UserId);

    /** Check if a user has a specific permission in a server (owner always has all) */
    bool UserHasPermission(Uint64 ServerId, Uint64 UserId, Uint64 Permission);

    /**
     * Channel operations
     *
     * AddChannel: creates a new channel and auto-assigns it the next available position
     *   (max position + 1) so it appears at the bottom of the channel list.
     *
     * RemoveChannel: deletes a channel and all its messages permanently.
     *   Requires CAN_MANAGE_CHANNELS permission (or server owner).
     *   Returns true on success, false if the channel wasn't found or permission denied.
     *
     * MoveChannel: changes a single channel's display position. All other channels are
     *   renumbered to fill gaps. NewPosition is clamped to the valid range.
     *   Requires CAN_MANAGE_CHANNELS permission (or server owner).
     *
     * ReorderChannels: batch reorder all channels at once by providing the complete
     *   ordered array of channel IDs. Designed for drag-and-drop UIs. The array must
     *   contain every channel in the server exactly once. Updates all positions in a
     *   single DB transaction for atomicity.
     *   Requires CAN_MANAGE_CHANNELS permission (or server owner).
     *
     * RenameChannel: changes a channel's name. Updates both DB and in-memory cache.
     *   Requires CAN_MANAGE_CHANNELS permission (or server owner).
     */
    Uint64 AddChannel(Uint64 ServerId, const std::string& ChannelName, EServerChannelType ChannelType, Uint64 RequestedByUserId);
    bool RemoveChannel(Uint64 ServerId, Uint64 ChannelId, Uint64 RequestedByUserId);
    bool MoveChannel(Uint64 ServerId, Uint64 ChannelId, uint32 NewPosition, Uint64 RequestedByUserId);
    bool ReorderChannels(Uint64 ServerId, const std::vector<Uint64>& ChannelIds, Uint64 RequestedByUserId);
    bool RenameChannel(Uint64 ServerId, Uint64 ChannelId, const std::string& NewName, Uint64 RequestedByUserId);

    /** Message operations */
    Uint64 AddMessage(Uint64 ServerId, Uint64 ChannelId, Uint64 SenderId, const std::string& SenderName, const std::string& Content, EMessageType MessageType = EMessageType::Text);
    std::vector<FServerMessage> GetChannelMessages(Uint64 ServerId, Uint64 ChannelId, Uint64 BeforeTimestamp, Uint32 Limit);

    /**
     * Delete a message from a server channel (hard delete: removed from DB and cache).
     *
     * Authorization: the message author or the server owner may delete a message.
     * Returns a result enum so the socket layer can produce a specific error.
     */
    EDeleteServerMessageResult DeleteMessage(Uint64 ServerId, Uint64 ChannelId, Uint64 MessageId, Uint64 RequestedByUserId);

    /**
     * Invite operations
     *
     * CreateInvite: generates a one-time-use or limited-use invite link.
     *  - MaxUses: maximum number of times the invite can be consumed (0 = use backend default)
     *  - ExpiresInSeconds: how long the invite lasts (0 = use backend default, capped at 12 months)
     *  - Subject to MaxInvitesPerServer cap (configurable in BackendSettings.ini, default 10).
     *  - Requires CAN_CREATE_INVITES permission (or owner).
     *
     * JoinViaInvite: consumes an invite code and adds the user to the server.
     *  - ClientIp: optional IP for abuse tracking. When set, failed attempts are counted
     *    and IPs are banned after MaxAttempts failures. Set to "" to bypass abuse protection.
     *  - OutError: if provided and the join fails, set to "invalid", "expired", "maxed_out",
     *    "abuse_ban", "already_member", or "server_not_found". On abuse_ban, the IP is
     *    banned for the configured duration.
     *
     * DeleteInvite: deletes an invite by its code.
     *  - Requires CAN_CREATE_INVITES permission (or owner).
     *  - Returns true if the invite was found and deleted, false otherwise.
     *
     * ListInvites: lists invites for a server with pagination.
     *  - Start: offset into the result set (0-based).
     *  - Count: maximum number of invites to return.
     *  - OutTotal: if non-null, receives the total number of invites for the server.
     *  - Requires CAN_CREATE_INVITES permission (or owner).
     */
    std::string CreateInvite(Uint64 ServerId, Uint64 CreatedByUserId, Uint32 MaxUses = 0, Uint32 ExpiresInSeconds = 0);
    std::shared_ptr<FServer> JoinViaInvite(const std::string& InviteCode, Uint64 UserId,
                                            const std::string& UserName,
                                            const std::string& ClientIp = "",
                                            std::string* OutError = nullptr);
    bool DeleteInvite(Uint64 ServerId, const std::string& InviteCode, Uint64 RequestedByUserId);
    std::vector<FInviteInfo> ListInvites(Uint64 ServerId, Uint64 RequestedByUserId,
                                         Uint32 Start, Uint32 Count, Uint32* OutTotal = nullptr);

    /** Voice channel operations */
    void JoinVoiceChannel(Uint64 ServerId, Uint64 ChannelId, Uint64 UserId);
    void LeaveVoiceChannel(Uint64 ServerId, Uint64 ChannelId, Uint64 UserId);

    /**
     * Get the list of user IDs currently connected to a specific voice channel.
     * Returns a thread-safe snapshot. Used to inform a newly joining user about
     * who is already connected to the channel.
     */
    std::vector<Uint64> GetVoiceChannelConnectedUsers(Uint64 ServerId, Uint64 ChannelId);

    /**
     * Get all voice channels a user is currently connected to.
     * Returns vector of {ServerId, ChannelId} pairs.
     * Used on WebSocket disconnect to auto-cleanup voice state.
     */
    std::vector<std::pair<Uint64, Uint64>> GetUserVoiceChannels(Uint64 UserId);

    /**
     * Remove a user from every voice channel they are connected to within a
     * single server (used when a member is kicked). Returns the list of channel
     * IDs the user was removed from, so the caller can broadcast voice-leave
     * events to the remaining members.
     */
    std::vector<Uint64> LeaveAllVoiceChannelsInServer(Uint64 ServerId, Uint64 UserId);

    /** Ensure a server is loaded into memory cache from DB */
    void EnsureServerLoaded(Uint64 ServerId);

protected:
    bool UploadNewServerToDB(const std::shared_ptr<FServer>& ServerPtr);
    bool DeleteServerFromDB(Uint64 InServerId);
    bool UploadChannelToDB(Uint64 ServerId, FServerChannel& Channel);
    bool DeleteChannelFromDB(Uint64 ServerId, Uint64 ChannelId);
    bool UpdateChannelNameInDB(Uint64 ServerId, Uint64 ChannelId, const std::string& NewName);
    bool UploadMessageToDB(const FServerMessage& Message, Uint64& OutMessageId);
    bool DeleteMessageFromDB(Uint64 MessageId, Uint64 ChannelId);
    bool GetServerMessageOwnerFromDB(Uint64 MessageId, Uint64& OutChannelId, Uint64& OutSenderId);
    bool UploadMemberToDB(Uint64 ServerId, Uint64 UserId, Uint64 Permissions);
    bool RemoveMemberFromDB(Uint64 ServerId, Uint64 UserId);
    bool UpdateMemberPermissionsInDB(Uint64 ServerId, Uint64 UserId, Uint64 NewPermissions);
    bool UploadInviteToDB(const std::string& InviteCode, Uint64 ServerId, Uint64 CreatedByUserId,
                          Uint32 MaxUses, Uint32 ExpiresInSeconds);
    bool ConsumeInviteFromDB(const std::string& InviteCode, Uint64& OutServerId);

    /**
     * Non-consuming lookup: resolves an invite code to its server_id in the DB
     * without validating expiry/usage limits and without incrementing current_uses.
     * Used by JoinViaInvite to check membership BEFORE consuming the invite.
     * Returns true if the invite code exists in the DB (regardless of validity),
     * false if not found or DB error.
     */
    bool LookupInviteServerId(const std::string& InviteCode, Uint64& OutServerId);

    bool DeleteInviteFromDB(const std::string& InviteCode, Uint64 ServerId);
    bool ListInvitesFromDB(Uint64 ServerId, Uint32 Start, Uint32 Count,
                           std::vector<FInviteInfo>& OutInvites, Uint32& OutTotal);

    /**
     * Count active (non-expired) invites for a server.
     * Used to enforce the MaxInvitesPerServer limit.
     */
    int32 GetActiveInviteCountForServer(Uint64 ServerId);

    /**
     * Get the next available position for a new channel.
     * Returns max(position) + 1 for channels in this server, or 0 if no channels exist.
     */
    uint32 GetNextChannelPosition(Uint64 ServerId);

    /**
     * Renumber all channel positions for a server sequentially (0, 1, 2, ...)
     * after a move or delete to eliminate gaps. Updates both DB and in-memory cache.
     */
    void RenumberChannelPositions(Uint64 ServerId);

    /** Download server data from DB into cache */
    bool DownloadServerFromDB(Uint64 ServerId);

    /** Download channels for a server from DB */
    bool DownloadChannelsFromDB(Uint64 ServerId, const std::shared_ptr<FServer>& Server);

    /** Download members for a server from DB (joins users table to get usernames and permissions) */
    bool DownloadMembersFromDB(Uint64 ServerId, const std::shared_ptr<FServer>& Server);

    /** Download messages for a channel from DB (timestamp-paginated) */
    bool DownloadMessagesFromDB(Uint64 ChannelId, const std::shared_ptr<FServer>& Server, Uint64 BeforeTimestamp = 0, Uint32 Limit = 50);

    /** Generate a unique token for a server */
    static std::string GenerateServerToken();

    /** Generate a unique invite code */
    static std::string GenerateInviteCode();

    /**
     * Format a std::chrono time_point as a MySQL TIMESTAMP string (UTC).
     * Returns "YYYY-MM-DD HH:MM:SS".
     */
    static std::string FormatTimestamp(const std::chrono::system_clock::time_point& Time);

private:
    /** Server Id to server instance map (in-memory cache) */
    std::unordered_map<Uint64, std::shared_ptr<FServer>> ServersMap;

    /** Mutex for servers map */
    mutable std::shared_mutex ServersMapMutex;

    /** Invite code to server_id map (in-memory cache with TTL) */
    std::unordered_map<std::string, Uint64> InviteCodeToServerId;

    /** Mutex for invite map */
    mutable std::shared_mutex InviteMapMutex;

    /**
     * Cache of user_id -> server IDs the user is a member of. Populated lazily
     * by GetUserServerIds() and invalidated on every membership mutation
     * (join/leave/kick/delete). Eliminates a synchronous DB SELECT from the hot
     * connect/disconnect path (BroadcastMemberStatus, CleanupUserVoiceChannels).
     */
    std::unordered_map<Uint64, std::vector<Uint64>> UserToServerIdsCache;

    /** Mutex for UserToServerIdsCache */
    mutable std::shared_mutex UserToServerIdsCacheMutex;

    /** Counter for periodic cleanup throttling (cleanup every ~300 ticks = 5 min) */
    int32 PostSecondTickCounter = 0;
};
