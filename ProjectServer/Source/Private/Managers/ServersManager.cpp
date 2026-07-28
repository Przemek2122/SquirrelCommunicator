// Created by https://www.linkedin.com/in/przemek2122/ 2020-2026

#include "Managers/ServersManager.h"
#include "DataBase/DataBaseConnect.h"
#include "Logger/Logger.h"
#include "SQRLLEncryption.h"

#include <soci/session.h>
#include <soci/statement.h>
#include <random>

FServersManager::FServersManager()
{
}

std::shared_ptr<FServer> FServersManager::GetServerById(const Uint64 InServerId)
{
    // Try cache first
    {
        std::shared_lock Lock(ServersMapMutex);
        auto ServerIter = ServersMap.find(InServerId);
        if (ServerIter != ServersMap.end())
        {
            return ServerIter->second;
        }
    }

    // Try loading from DB
    if (DownloadServerFromDB(InServerId))
    {
        std::shared_lock Lock(ServersMapMutex);
        auto ServerIter = ServersMap.find(InServerId);
        if (ServerIter != ServersMap.end())
        {
            return ServerIter->second;
        }
    }

    return nullptr;
}

std::vector<std::shared_ptr<FServer>> FServersManager::GetUserServers(const Uint64 UserId)
{
    std::vector<std::shared_ptr<FServer>> Result;

    const std::vector<Uint64> ServerIds = GetUserServerIds(UserId);

    // Load each server (from cache or DB)
    for (const Uint64 Id : ServerIds)
    {
        const std::shared_ptr<FServer> Server = GetServerById(Id);
        if (Server)
        {
            Result.push_back(Server);
        }
    }

    return Result;
}

std::vector<Uint64> FServersManager::GetUserServerIds(Uint64 UserId)
{
    std::vector<Uint64> ServerIds;

    FDataBaseConnect Connect;
    if (!Connect.IsConnected())
    {
        LOG_ERROR("Database connection failed in GetUserServerIds");
        return ServerIds;
    }

    try
    {
        soci::session& Session = Connect.GetSession();

        Uint64 ServerId = 0;

        soci::statement St = (Session.prepare <<
            "SELECT server_id FROM server_members WHERE user_id = :uid",
            soci::into(ServerId),
            soci::use(UserId));

        St.execute();
        while (St.fetch())
        {
            ServerIds.push_back(ServerId);
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("GetUserServerIds DB error: " << e.what());
    }

    return ServerIds;
}

Uint64 FServersManager::AddServer(const std::string& InServerName, Uint64 OwnerId)
{
    if (InServerName.empty() || OwnerId == 0)
    {
        LOG_ERROR("AddServer: Invalid parameters");
        return 0;
    }

    const std::shared_ptr<FServer> NewServer = std::make_shared<FServer>();
    NewServer->SetServerName(InServerName);
    NewServer->SetOwnerId(OwnerId);
    NewServer->SetToken(GenerateServerToken());

    // Get current time as ISO string (simplified: use Unix timestamp as string for now)
    NewServer->SetCreatedAt(std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));

    // Upload to DB (this also sets the server ID via auto-increment)
    if (!UploadNewServerToDB(NewServer))
    {
        LOG_ERROR("AddServer: Failed to upload to DB");
        return 0;
    }

    const Uint64 NewServerId = NewServer->GetServerId();

    // Add owner as member
    FServerMember OwnerMember;
    OwnerMember.UserId = OwnerId;
    OwnerMember.Status = "online";
    NewServer->AddMember(OwnerMember);
    UploadMemberToDB(NewServerId, OwnerId);

    // Create default channels
    AddChannel(NewServerId, "general", EServerChannelType::Text);
    AddChannel(NewServerId, "General", EServerChannelType::Voice);

    // Add to cache
    {
        std::unique_lock Lock(ServersMapMutex);
        ServersMap[NewServerId] = NewServer;
    }

    LOG_INFO("Created server '" << InServerName << "' with ID " << NewServerId);
    return NewServerId;
}

bool FServersManager::RemoveServer(const Uint64 InServerId)
{
    if (!DeleteServerFromDB(InServerId))
    {
        LOG_ERROR("RemoveServer: Failed to delete from DB");
        return false;
    }

    {
        std::unique_lock Lock(ServersMapMutex);
        ServersMap.erase(InServerId);
    }

    return true;
}

bool FServersManager::AddUserToServer(Uint64 ServerId, Uint64 UserId, const std::string& UserName)
{
    auto Server = GetServerById(ServerId);
    if (!Server)
    {
        LOG_ERROR("AddUserToServer: Server not found: " << ServerId);
        return false;
    }

    if (Server->HasMember(UserId))
    {
        return true; // Already a member
    }

    if (!UploadMemberToDB(ServerId, UserId))
    {
        LOG_ERROR("AddUserToServer: Failed to upload member to DB");
        return false;
    }

    FServerMember Member;
    Member.UserId = UserId;
    Member.UserName = UserName;
    Member.Status = "online";
    Server->AddMember(Member);

    return true;
}

bool FServersManager::RemoveUserFromServer(const Uint64 ServerId, const Uint64 UserId)
{
    auto Server = GetServerById(ServerId);
    if (!Server)
    {
        return false;
    }

    if (!RemoveMemberFromDB(ServerId, UserId))
    {
        LOG_ERROR("RemoveUserFromServer: Failed to remove member from DB");
        return false;
    }

    Server->RemoveMember(UserId);
    return true;
}

bool FServersManager::IsUserInServer(const Uint64 ServerId, const Uint64 UserId)
{
    auto Server = GetServerById(ServerId);
    if (!Server)
    {
        return false;
    }
    return Server->HasMember(UserId);
}

Uint64 FServersManager::AddChannel(Uint64 ServerId, const std::string& ChannelName, EServerChannelType ChannelType)
{
    auto Server = GetServerById(ServerId);
    if (!Server)
    {
        LOG_ERROR("AddChannel: Server not found: " << ServerId);
        return 0;
    }

    FServerChannel Channel;
    Channel.ServerId = ServerId;
    Channel.ChannelName = ChannelName;
    Channel.ChannelType = ChannelType;

    // Upload to DB first to get channel ID
    if (!UploadChannelToDB(ServerId, Channel))
    {
        LOG_ERROR("AddChannel: Failed to upload channel to DB");
        return 0;
    }

    Server->AddChannel(Channel);
    return Channel.ChannelId;
}

bool FServersManager::RemoveChannel(Uint64 ServerId, Uint64 ChannelId)
{
    auto Server = GetServerById(ServerId);
    if (!Server)
    {
        return false;
    }

    FDataBaseConnect Connect;
    if (!Connect.IsConnected())
    {
        return false;
    }

    try
    {
        soci::session& Session = Connect.GetSession();
        Session << "DELETE FROM server_channels WHERE id = :cid AND server_id = :sid",
            soci::use(ChannelId),
            soci::use(ServerId);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("RemoveChannel DB error: " << e.what());
        return false;
    }

    return Server->RemoveChannel(ChannelId);
}

Uint64 FServersManager::AddMessage(Uint64 ServerId, Uint64 ChannelId, Uint64 SenderId, const std::string& SenderName, const std::string& Content)
{
    auto Server = GetServerById(ServerId);
    if (!Server)
    {
        LOG_ERROR("AddMessage: Server not found: " << ServerId);
        return 0;
    }

    FServerMessage Message;
    Message.ChannelId = ChannelId;
    Message.SenderId = SenderId;
    Message.SenderName = SenderName;
    Message.Content = Content;
    Message.CreatedAt = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

    Uint64 OutMessageId = 0;
    if (!UploadMessageToDB(Message, OutMessageId))
    {
        LOG_ERROR("AddMessage: Failed to upload message to DB");
        return 0;
    }

    Message.MessageId = OutMessageId;
    Server->AddMessage(Message);

    return OutMessageId;
}

std::vector<FServerMessage> FServersManager::GetChannelMessages(const Uint64 ServerId, const Uint64 ChannelId, const Uint64 BeforeTimestamp, const int32 Limit)
{
    const std::shared_ptr<FServer> Server = GetServerById(ServerId);
    if (!Server)
    {
        return {};
    }

    // Try cache first
    std::vector<FServerMessage> CachedMessages = Server->GetChannelMessages(ChannelId, BeforeTimestamp, Limit);
    if (!CachedMessages.empty())
    {
        return CachedMessages;
    }

    // Load from DB with timestamp pagination
    DownloadMessagesFromDB(ChannelId, Server, BeforeTimestamp, Limit);
    return Server->GetChannelMessages(ChannelId, BeforeTimestamp, Limit);
}

std::string FServersManager::CreateInvite(const Uint64 ServerId, const Uint64 CreatedByUserId)
{
    const std::shared_ptr<FServer> Server = GetServerById(ServerId);
    if (!Server)
    {
        LOG_ERROR("CreateInvite: Server not found: " << ServerId);
        return "";
    }

    std::string InviteCode = GenerateInviteCode();

    if (!UploadInviteToDB(InviteCode, ServerId, CreatedByUserId))
    {
        LOG_ERROR("CreateInvite: Failed to upload invite to DB");
        return "";
    }

    {
        std::unique_lock Lock(InviteMapMutex);
        InviteCodeToServerId[InviteCode] = ServerId;
    }

    return InviteCode;
}

std::shared_ptr<FServer> FServersManager::JoinViaInvite(const std::string& InviteCode, Uint64 UserId, const std::string& UserName)
{
    Uint64 ServerId = 0;

    // Check in-memory cache first
    {
        std::shared_lock Lock(InviteMapMutex);
        auto Iter = InviteCodeToServerId.find(InviteCode);
        if (Iter != InviteCodeToServerId.end())
        {
            ServerId = Iter->second;
        }
    }

    // If not in cache, query DB
    if (ServerId == 0)
    {
        if (!ConsumeInviteFromDB(InviteCode, ServerId))
        {
            LOG_ERROR("JoinViaInvite: Invalid or expired invite code");
            return nullptr;
        }
    }

    const std::shared_ptr<FServer> Server = GetServerById(ServerId);
    if (!Server)
    {
        LOG_ERROR("JoinViaInvite: Server not found: " << ServerId);
        return nullptr;
    }

    if (!AddUserToServer(ServerId, UserId, UserName))
    {
        LOG_ERROR("JoinViaInvite: Failed to add user to server");
        return nullptr;
    }

    return Server;
}

void FServersManager::JoinVoiceChannel(const Uint64 ServerId, const Uint64 ChannelId, const Uint64 UserId)
{
    const std::shared_ptr<FServer> Server = GetServerById(ServerId);
    if (!Server)
    {
        return;
    }

    const std::shared_ptr<FServerChannel> Channel = Server->GetChannel(ChannelId);
    if (!Channel || Channel->ChannelType != EServerChannelType::Voice)
    {
        return;
    }

    std::unique_lock Lock(Server->GetMutex());
    // Add user to connected users if not already there
    std::vector<Uint64>& Users = Channel->ConnectedUsers;
    if (std::find(Users.begin(), Users.end(), UserId) == Users.end())
    {
        Users.push_back(UserId);
    }
}

void FServersManager::LeaveVoiceChannel(const Uint64 ServerId, const Uint64 ChannelId, const Uint64 UserId)
{
    const std::shared_ptr<FServer> Server = GetServerById(ServerId);
    if (!Server)
    {
        return;
    }

    const std::shared_ptr<FServerChannel> Channel = Server->GetChannel(ChannelId);
    if (!Channel)
    {
        return;
    }

    std::unique_lock Lock(Server->GetMutex());
    std::vector<Uint64>& Users = Channel->ConnectedUsers;
    Users.erase(std::remove(Users.begin(), Users.end(), UserId), Users.end());
}

std::vector<std::pair<Uint64, Uint64>> FServersManager::GetUserVoiceChannels(Uint64 UserId)
{
    std::vector<std::pair<Uint64, Uint64>> Result;

    // Get all server IDs this user is a member of
    const std::vector<Uint64> ServerIds = GetUserServerIds(UserId);

    for (Uint64 ServerId : ServerIds)
    {
        const std::shared_ptr<FServer> Server = GetServerById(ServerId);
        if (!Server)
        {
            continue;
        }

        // Check each voice channel for this user
        std::vector<std::shared_ptr<FServerChannel>> Channels = Server->GetAllChannels();
        for (const std::shared_ptr<FServerChannel>& Channel : Channels)
        {
            if (Channel->ChannelType != EServerChannelType::Voice)
            {
                continue;
            }

            // Check if user is in ConnectedUsers
            const std::vector<Uint64>& Users = Channel->ConnectedUsers;
            if (std::find(Users.begin(), Users.end(), UserId) != Users.end())
            {
                Result.emplace_back(ServerId, Channel->ChannelId);
            }
        }
    }

    return Result;
}

void FServersManager::EnsureServerLoaded(const Uint64 ServerId)
{
    GetServerById(ServerId); // This triggers download if not cached
}

// ========== DB Operations ==========

bool FServersManager::UploadNewServerToDB(const std::shared_ptr<FServer>& ServerPtr)
{
    FDataBaseConnect Connect;
    if (!Connect.IsConnected())
    {
        LOG_ERROR("UploadNewServerToDB: No DB connection");
        return false;
    }

    try
    {
        soci::session& Session = Connect.GetSession();

        Session << "INSERT INTO servers (name, owner_id, token, created_at) VALUES (:name, :owner, :token, :created)",
            soci::use(ServerPtr->GetServerName()),
            soci::use(ServerPtr->GetOwnerId()),
            soci::use(ServerPtr->GetToken()),
            soci::use(ServerPtr->GetCreatedAt());

        // Get the last inserted ID
        long long LastId = 0;
        Session.get_last_insert_id("servers", LastId);
        ServerPtr->SetServerId(static_cast<Uint64>(LastId));

        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("UploadNewServerToDB error: " << e.what());
        return false;
    }
}

bool FServersManager::DeleteServerFromDB(Uint64 InServerId)
{
    FDataBaseConnect Connect;
    if (!Connect.IsConnected())
    {
        return false;
    }

    try
    {
        soci::session& Session = Connect.GetSession();

        // Delete in order: messages → channels → members → invites → server
        Session << "DELETE FROM server_messages WHERE channel_id IN (SELECT id FROM server_channels WHERE server_id = :sid)",
            soci::use(InServerId);
        Session << "DELETE FROM server_channels WHERE server_id = :sid",
            soci::use(InServerId);
        Session << "DELETE FROM server_members WHERE server_id = :sid",
            soci::use(InServerId);
        Session << "DELETE FROM server_invites WHERE server_id = :sid",
            soci::use(InServerId);
        Session << "DELETE FROM servers WHERE id = :sid",
            soci::use(InServerId);

        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("DeleteServerFromDB error: " << e.what());
        return false;
    }
}

bool FServersManager::UploadChannelToDB(Uint64 ServerId, FServerChannel& Channel)
{
    FDataBaseConnect Connect;
    if (!Connect.IsConnected())
    {
        return false;
    }

    try
    {
        soci::session& Session = Connect.GetSession();

        std::string ChannelTypeStr = (Channel.ChannelType == EServerChannelType::Text) ? "text" : "voice";

        Session << "INSERT INTO server_channels (server_id, name, type) VALUES (:sid, :name, :type)",
            soci::use(ServerId),
            soci::use(Channel.ChannelName),
            soci::use(ChannelTypeStr);

        long long LastId = 0;
        Session.get_last_insert_id("server_channels", LastId);
        Channel.ChannelId = static_cast<Uint64>(LastId);

        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("UploadChannelToDB error: " << e.what());
        return false;
    }
}

bool FServersManager::UploadMessageToDB(const FServerMessage& Message, Uint64& OutMessageId)
{
    FDataBaseConnect Connect;
    if (!Connect.IsConnected())
    {
        return false;
    }

    try
    {
        soci::session& Session = Connect.GetSession();

        Session << "INSERT INTO server_messages (channel_id, sender_id, content, created_at) VALUES (:cid, :sid, :content, :created)",
            soci::use(Message.ChannelId),
            soci::use(Message.SenderId),
            soci::use(Message.Content),
            soci::use(Message.CreatedAt);

        long long LastId = 0;
        Session.get_last_insert_id("server_messages", LastId);
        OutMessageId = static_cast<Uint64>(LastId);

        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("UploadMessageToDB error: " << e.what());
        return false;
    }
}

bool FServersManager::UploadMemberToDB(Uint64 ServerId, Uint64 UserId)
{
    FDataBaseConnect Connect;
    if (!Connect.IsConnected())
    {
        return false;
    }

    try
    {
        soci::session& Session = Connect.GetSession();
        Session << "INSERT IGNORE INTO server_members (server_id, user_id) VALUES (:sid, :uid)",
            soci::use(ServerId),
            soci::use(UserId);
        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("UploadMemberToDB error: " << e.what());
        return false;
    }
}

bool FServersManager::RemoveMemberFromDB(Uint64 ServerId, Uint64 UserId)
{
    FDataBaseConnect Connect;
    if (!Connect.IsConnected())
    {
        return false;
    }

    try
    {
        soci::session& Session = Connect.GetSession();
        Session << "DELETE FROM server_members WHERE server_id = :sid AND user_id = :uid",
            soci::use(ServerId),
            soci::use(UserId);
        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("RemoveMemberFromDB error: " << e.what());
        return false;
    }
}

bool FServersManager::UploadInviteToDB(const std::string& InviteCode, Uint64 ServerId, Uint64 CreatedByUserId)
{
    FDataBaseConnect Connect;
    if (!Connect.IsConnected())
    {
        return false;
    }

    try
    {
        soci::session& Session = Connect.GetSession();
        Session << "INSERT INTO server_invites (invite_code, server_id, created_by) VALUES (:code, :sid, :uid)",
            soci::use(InviteCode),
            soci::use(ServerId),
            soci::use(CreatedByUserId);
        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("UploadInviteToDB error: " << e.what());
        return false;
    }
}

bool FServersManager::ConsumeInviteFromDB(const std::string& InviteCode, Uint64& OutServerId)
{
    FDataBaseConnect Connect;
    if (!Connect.IsConnected())
    {
        return false;
    }

    try
    {
        soci::session& Session = Connect.GetSession();

        long long ServerId = 0;
        soci::indicator Ind;

        Session << "SELECT server_id FROM server_invites WHERE invite_code = :code",
            soci::into(ServerId, Ind),
            soci::use(InviteCode);

        if (Session.got_data() && Ind == soci::i_ok)
        {
            OutServerId = static_cast<Uint64>(ServerId);

            // Optionally: delete the invite after use (single-use)
            // Session << "DELETE FROM server_invites WHERE invite_code = :code", soci::use(InviteCode);

            return true;
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("ConsumeInviteFromDB error: " << e.what());
    }

    return false;
}

bool FServersManager::DownloadServerFromDB(Uint64 ServerId)
{
    FDataBaseConnect Connect;
    if (!Connect.IsConnected())
    {
        return false;
    }

    try
    {
        soci::session& Session = Connect.GetSession();

        std::string Name, Token, CreatedAt;
        long long OwnerId = 0;
        soci::indicator IndName, IndToken, IndCreated;

        Session << "SELECT name, owner_id, token, created_at FROM servers WHERE id = :sid",
            soci::into(Name, IndName),
            soci::into(OwnerId),
            soci::into(Token, IndToken),
            soci::into(CreatedAt, IndCreated),
            soci::use(ServerId);

        if (!Session.got_data())
        {
            return false;
        }

        const std::shared_ptr<FServer> Server = std::make_shared<FServer>();
        Server->SetServerId(ServerId);
        Server->SetServerName(Name);
        Server->SetOwnerId(static_cast<Uint64>(OwnerId));
        Server->SetToken(Token);
        Server->SetCreatedAt(CreatedAt);

        // Download channels
        DownloadChannelsFromDB(ServerId, Server);

        // Download members (with usernames from users table)
        DownloadMembersFromDB(ServerId, Server);

        {
            std::unique_lock Lock(ServersMapMutex);
            ServersMap[ServerId] = Server;
        }

        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("DownloadServerFromDB error: " << e.what());
        return false;
    }
}

bool FServersManager::DownloadChannelsFromDB(Uint64 ServerId, const std::shared_ptr<FServer>& Server)
{
    FDataBaseConnect Connect;
    if (!Connect.IsConnected())
    {
        return false;
    }

    try
    {
        soci::session& Session = Connect.GetSession();

        long long ChannelId = 0;
        std::string ChannelName, ChannelType;
        soci::indicator IndId, IndName, IndType;

        soci::statement St = (Session.prepare <<
            "SELECT id, name, type FROM server_channels WHERE server_id = :sid",
            soci::into(ChannelId, IndId),
            soci::into(ChannelName, IndName),
            soci::into(ChannelType, IndType),
            soci::use(ServerId));

        St.execute();
        while (St.fetch())
        {
            FServerChannel Channel;
            Channel.ChannelId = static_cast<Uint64>(ChannelId);
            Channel.ServerId = ServerId;
            Channel.ChannelName = ChannelName;
            Channel.ChannelType = (ChannelType == "voice") ? EServerChannelType::Voice : EServerChannelType::Text;
            Server->AddChannel(Channel);
        }

        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("DownloadChannelsFromDB error: " << e.what());
        return false;
    }
}

bool FServersManager::DownloadMembersFromDB(Uint64 ServerId, std::shared_ptr<FServer> Server)
{
    FDataBaseConnect Connect;
    if (!Connect.IsConnected())
    {
        return false;
    }

    try
    {
        soci::session& Session = Connect.GetSession();

        long long UserId = 0;
        std::string UserName;
        soci::indicator IndUserId, IndUserName;

        // JOIN with users table to get usernames
        soci::statement St = (Session.prepare <<
            "SELECT sm.user_id, u.username FROM server_members sm "
            "JOIN users u ON sm.user_id = u.id "
            "WHERE sm.server_id = :sid",
            soci::into(UserId, IndUserId),
            soci::into(UserName, IndUserName),
            soci::use(ServerId));

        St.execute();
        while (St.fetch())
        {
            FServerMember Member;
            Member.UserId = static_cast<Uint64>(UserId);
            Member.UserName = IndUserName == soci::i_ok ? UserName : "";
            Member.Status = "offline"; // Default; will be updated via WebSocket status events
            Server->AddMember(Member);
        }

        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("DownloadMembersFromDB error: " << e.what());
        return false;
    }
}

bool FServersManager::DownloadMessagesFromDB(Uint64 ChannelId, std::shared_ptr<FServer> Server, Uint64 BeforeTimestamp, int32 Limit)
{
    FDataBaseConnect Connect;
    if (!Connect.IsConnected())
    {
        return false;
    }

    try
    {
        soci::session& Session = Connect.GetSession();

        long long MessageId = 0, SenderId = 0;
        std::string Content, CreatedAt, SenderName;
        soci::indicator IndMsgId, IndSenderId, IndContent, IndCreated, IndSenderName;

        // Pagination: fetch messages before a timestamp, newest first
        if (BeforeTimestamp > 0)
        {
            soci::statement St = (Session.prepare <<
                "SELECT sm.id, sm.sender_id, u.username, sm.content, sm.created_at "
                "FROM server_messages sm "
                "JOIN users u ON sm.sender_id = u.id "
                "WHERE sm.channel_id = :cid AND sm.created_at < :before "
                "ORDER BY sm.id DESC LIMIT :lim",
                soci::into(MessageId, IndMsgId),
                soci::into(SenderId, IndSenderId),
                soci::into(SenderName, IndSenderName),
                soci::into(Content, IndContent),
                soci::into(CreatedAt, IndCreated),
                soci::use(ChannelId),
                soci::use(std::to_string(BeforeTimestamp)),
                soci::use(Limit));

            St.execute();
            while (St.fetch())
            {
                FServerMessage Message;
                Message.MessageId = static_cast<Uint64>(MessageId);
                Message.ChannelId = ChannelId;
                Message.SenderId = static_cast<Uint64>(SenderId);
                Message.SenderName = IndSenderName == soci::i_ok ? SenderName : "";
                Message.Content = Content;
                Message.CreatedAt = CreatedAt;
                Server->AddMessage(Message);
            }
        }
        else
        {
            // No timestamp filter: get the most recent messages
            soci::statement St = (Session.prepare <<
                "SELECT sm.id, sm.sender_id, u.username, sm.content, sm.created_at "
                "FROM server_messages sm "
                "JOIN users u ON sm.sender_id = u.id "
                "WHERE sm.channel_id = :cid "
                "ORDER BY sm.id DESC LIMIT :lim",
                soci::into(MessageId, IndMsgId),
                soci::into(SenderId, IndSenderId),
                soci::into(SenderName, IndSenderName),
                soci::into(Content, IndContent),
                soci::into(CreatedAt, IndCreated),
                soci::use(ChannelId),
                soci::use(Limit));

            St.execute();
            while (St.fetch())
            {
                FServerMessage Message;
                Message.MessageId = static_cast<Uint64>(MessageId);
                Message.ChannelId = ChannelId;
                Message.SenderId = static_cast<Uint64>(SenderId);
                Message.SenderName = IndSenderName == soci::i_ok ? SenderName : "";
                Message.Content = Content;
                Message.CreatedAt = CreatedAt;
                Server->AddMessage(Message);
            }
        }

        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("DownloadMessagesFromDB error: " << e.what());
        return false;
    }
}

// ========== Helper Methods ==========

static constexpr std::string_view Chars =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

std::string FServersManager::GenerateServerToken()
{
    std::random_device Rd;
    std::mt19937 Gen(Rd());
    std::uniform_int_distribution<size_t> Dis(0, Chars.size() - 1);

    std::string Token;
    Token.reserve(48);
    for (int i = 0; i < 48; ++i)
    {
        Token += Chars[Dis(Gen)];
    }

    return Token;
}

std::string FServersManager::GenerateInviteCode()
{
    std::random_device Rd;
    std::mt19937 Gen(Rd());
    std::uniform_int_distribution<size_t> Dis(0, Chars.size() - 1);

    std::string Code;
    Code.reserve(10);
    for (int i = 0; i < 10; ++i)
    {
        Code += Chars[Dis(Gen)];
    }

    return Code;
}
