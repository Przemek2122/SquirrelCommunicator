// Created by https://www.linkedin.com/in/przemek2122/ 2020-2026

#pragma once

#include "EngineCompat.h"
#include <shared_mutex>
#include <vector>
#include <unordered_map>

/** Type of channel within a server */
enum class EServerChannelType : uint8
{
    Text,
    Voice
};

/** Represents a single channel within a server */
struct FServerChannel
{
    Uint64 ChannelId = 0;
    Uint64 ServerId = 0;
    std::string ChannelName;
    EServerChannelType ChannelType = EServerChannelType::Text;

    /** For voice channels: set of user IDs currently connected */
    std::vector<Uint64> ConnectedUsers;

    FServerChannel() = default;
};

/** Represents a single message in a server text channel */
struct FServerMessage
{
    Uint64 MessageId = 0;
    Uint64 ChannelId = 0;
    Uint64 SenderId = 0;
    std::string SenderName;
    std::string Content;
    std::string CreatedAt; // Unix timestamp as string (epoch nanoseconds)

    FServerMessage() = default;
};

/** Member of a server with status */
struct FServerMember
{
    Uint64 UserId = 0;
    std::string UserName;
    std::string Status; // "online", "offline", "away"

    FServerMember() = default;
};

/** Represents single-server instance */
class FServer
{
public:
    FServer();
    ~FServer() = default;

    Uint64 GetServerId() const { return ServerId; }
    void SetServerId(Uint64 InId) { ServerId = InId; }

    const std::string& GetServerName() const { return ServerName; }
    void SetServerName(const std::string& InName) { ServerName = InName; }

    const Uint64& GetOwnerId() const { return OwnerId; }
    void SetOwnerId(Uint64 InOwnerId) { OwnerId = InOwnerId; }

    const std::string& GetToken() const { return Token; }
    void SetToken(const std::string& InToken) { Token = InToken; }

    const std::string& GetCreatedAt() const { return CreatedAt; }
    void SetCreatedAt(const std::string& InTime) { CreatedAt = InTime; }

    /** Channel management */
    void AddChannel(const FServerChannel& Channel);
    bool RemoveChannel(Uint64 ChannelId);
    std::shared_ptr<FServerChannel> GetChannel(Uint64 ChannelId);
    std::vector<std::shared_ptr<FServerChannel>> GetAllChannels();

    /** Member management */
    void AddMember(const FServerMember& Member);
    bool RemoveMember(Uint64 UserId);
    bool HasMember(Uint64 UserId) const;
    void UpdateMemberStatus(Uint64 UserId, const std::string& NewStatus);
    void UpdateMemberUserName(Uint64 UserId, const std::string& UserName);
    std::vector<FServerMember> GetMembers();
    size_t GetMemberCount() const;

    /** Messages (stored per-channel in memory cache, newest-first) */
    void AddMessage(const FServerMessage& Message);

    /** Get messages before a timestamp. BeforeTimestamp=0 means no filter (get most recent). */
    std::vector<FServerMessage> GetChannelMessages(Uint64 ChannelId, Uint64 BeforeTimestamp, int32 Limit);

    /** Thread-safe access */
    std::shared_mutex& GetMutex() { return ServerMutex; }

private:
    /** Server id */
    Uint64 ServerId = 0;

    /** Displayed server name */
    std::string ServerName;

    /** Owner user ID */
    Uint64 OwnerId = 0;

    /** Token for voice service / invites */
    std::string Token;

    /** ISO timestamp of creation */
    std::string CreatedAt;

    /** Channels: channel_id → channel */
    std::unordered_map<Uint64, std::shared_ptr<FServerChannel>> Channels;

    /** Members: user_id → member info */
    std::unordered_map<Uint64, FServerMember> Members;

    /** Messages cache: channel_id → vector of messages (most recent first by MessageId) */
    std::unordered_map<Uint64, std::vector<FServerMessage>> ChannelMessages;

    /** Server mutex for thread-safe access */
    mutable std::shared_mutex ServerMutex;
};
