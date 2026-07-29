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

/**
 * Server member permissions (bitfield).
 * Stored as Uint64. Each bit controls one capability.
 */
namespace EServerPermission
{
    /** Allow member to create invite codes for this server */
    constexpr Uint64 CAN_CREATE_INVITES = 1ULL << 0;

    /** Allow member to kick other members (future) */
    constexpr Uint64 CAN_KICK_MEMBERS   = 1ULL << 1;

    /** Allow member to ban other members (future) */
    constexpr Uint64 CAN_BAN_MEMBERS    = 1ULL << 2;

    /** Allow member to create, edit, delete channels (future) */
    constexpr Uint64 CAN_MANAGE_CHANNELS = 1ULL << 3;

    /** Allow member to manage other members permissions (future) */
    constexpr Uint64 CAN_MANAGE_PERMISSIONS = 1ULL << 4;

    /** All permissions (granted to server owner by default) */
    constexpr Uint64 ALL_PERMISSIONS = 0xFFFFFFFFFFFFFFFFULL;

    /** Helper: check if a permission bit is set */
    inline bool HasPermission(Uint64 Permissions, Uint64 Permission)
    {
        return (Permissions & Permission) != 0;
    }

    /** Helper: grant a permission */
    inline void Grant(Uint64& Permissions, Uint64 Permission)
    {
        Permissions |= Permission;
    }

    /** Helper: revoke a permission */
    inline void Revoke(Uint64& Permissions, Uint64 Permission)
    {
        Permissions &= ~Permission;
    }
}

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

/** Member of a server with status and permissions */
struct FServerMember
{
    Uint64 UserId = 0;
    std::string UserName;
    std::string Status; // "online", "offline", "away"
    Uint64 Permissions = 0; // Bitfield of EServerPermission flags

    FServerMember() = default;

    /** Check if this member has a specific permission */
    bool HasPermission(Uint64 Permission) const
    {
        return EServerPermission::HasPermission(Permissions, Permission);
    }
};

/**
 * Lightweight invite metadata returned by ListInvites.
 * Used for displaying invite management UI in the frontend.
 */
struct FInviteInfo
{
    std::string InviteCode;    // The alphanumeric invite token
    Uint64 CreatedBy = 0;      // User ID who created the invite
    Uint32 MaxUses = 0;         // Configured max usage count
    Uint32 CurrentUses = 0;     // How many times it's been consumed
    std::string CreatedAt;     // Creation timestamp (MySQL TIMESTAMP)
    std::string ExpiresAt;     // Expiration timestamp (MySQL TIMESTAMP)

    FInviteInfo() = default;

    /** Remaining uses before the invite is exhausted. Clamped to zero defensively. */
    Uint32 RemainingUses() const
    {
        return (MaxUses > CurrentUses) ? (MaxUses - CurrentUses) : 0;
    }

    /** Whether the invite has no remaining uses */
    [[nodiscard]] bool IsExhausted() const
    {
        return CurrentUses >= MaxUses;
    }
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
    void UpdateMemberPermissions(Uint64 UserId, Uint64 NewPermissions);
    Uint64 GetMemberPermissions(Uint64 UserId) const;
    std::vector<FServerMember> GetMembers() const;
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

    /** Channels: channel_id to channel */
    std::unordered_map<Uint64, std::shared_ptr<FServerChannel>> Channels;

    /** Members: user_id to member info */
    std::unordered_map<Uint64, FServerMember> Members;

    /** Messages cache: channel_id to vector of messages (most recent first by MessageId) */
    std::unordered_map<Uint64, std::vector<FServerMessage>> ChannelMessages;

    /** Server mutex for thread-safe access */
    mutable std::shared_mutex ServerMutex;
};
