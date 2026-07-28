// Created by https://www.linkedin.com/in/przemek2122/ 2020-2026

#pragma once

#include "EngineCompat.h"
#include "Managers/Server.h"

#include <shared_mutex>
#include <unordered_map>
#include <vector>

class FServer;

/**
 * Manager for user servers (Discord-like "Rooms" from the frontend perspective).
 * Handles:
 *  - Server creation and deletion
 *  - Server membership (join/leave/invite)
 *  - Channel CRUD within servers
 *  - Messages within text channels
 *  - Invite code generation and resolution
 *  - Database persistence for all server data
 */
class FServersManager
{
public:
    FServersManager();

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
    bool AddUserToServer(Uint64 ServerId, Uint64 UserId, const std::string& UserName);
    bool RemoveUserFromServer(Uint64 ServerId, Uint64 UserId);
    bool IsUserInServer(Uint64 ServerId, Uint64 UserId);

    /** Channel operations */
    Uint64 AddChannel(Uint64 ServerId, const std::string& ChannelName, EServerChannelType ChannelType);
    bool RemoveChannel(Uint64 ServerId, Uint64 ChannelId);

    /** Message operations */
    Uint64 AddMessage(Uint64 ServerId, Uint64 ChannelId, Uint64 SenderId, const std::string& SenderName, const std::string& Content);
    std::vector<FServerMessage> GetChannelMessages(Uint64 ServerId, Uint64 ChannelId, Uint64 BeforeTimestamp, int32 Limit);

    /** Invite operations */
    std::string CreateInvite(Uint64 ServerId, Uint64 CreatedByUserId);
    std::shared_ptr<FServer> JoinViaInvite(const std::string& InviteCode, Uint64 UserId, const std::string& UserName);

    /** Voice channel operations */
    void JoinVoiceChannel(Uint64 ServerId, Uint64 ChannelId, Uint64 UserId);
    void LeaveVoiceChannel(Uint64 ServerId, Uint64 ChannelId, Uint64 UserId);

    /** Ensure a server is loaded into memory cache from DB */
    void EnsureServerLoaded(Uint64 ServerId);

protected:
    bool UploadNewServerToDB(const std::shared_ptr<FServer>& ServerPtr);
    bool DeleteServerFromDB(Uint64 InServerId);
    bool UploadChannelToDB(Uint64 ServerId, FServerChannel& Channel);
    bool UploadMessageToDB(const FServerMessage& Message, Uint64& OutMessageId);
    bool UploadMemberToDB(Uint64 ServerId, Uint64 UserId);
    bool RemoveMemberFromDB(Uint64 ServerId, Uint64 UserId);
    bool UploadInviteToDB(const std::string& InviteCode, Uint64 ServerId, Uint64 CreatedByUserId);
    bool ConsumeInviteFromDB(const std::string& InviteCode, Uint64& OutServerId);

    /** Download server data from DB into cache */
    bool DownloadServerFromDB(Uint64 ServerId);

    /** Download channels for a server from DB */
    bool DownloadChannelsFromDB(Uint64 ServerId, const std::shared_ptr<FServer>& Server);

    /** Download members for a server from DB (joins users table to get usernames) */
    bool DownloadMembersFromDB(Uint64 ServerId, std::shared_ptr<FServer> Server);

    /** Download messages for a channel from DB (timestamp-paginated) */
    bool DownloadMessagesFromDB(Uint64 ChannelId, std::shared_ptr<FServer> Server, Uint64 BeforeTimestamp = 0, int32 Limit = 50);

    /** Generate a unique token for a server */
    static std::string GenerateServerToken();

    /** Generate a unique invite code */
    static std::string GenerateInviteCode();

private:
    /** Server Id to server instance map (in-memory cache) */
    std::unordered_map<Uint64, std::shared_ptr<FServer>> ServersMap;

    /** Mutex for servers map */
    mutable std::shared_mutex ServersMapMutex;

    /** Invite code → server_id map (in-memory cache with TTL) */
    std::unordered_map<std::string, Uint64> InviteCodeToServerId;

    /** Mutex for invite map */
    mutable std::shared_mutex InviteMapMutex;
};
