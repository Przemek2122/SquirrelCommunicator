// Created by https://www.linkedin.com/in/przemek2122/ 2020-2026

#pragma once

#include "EngineCompat.h"
#include "Managers/MessageType.h"
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

    /** Allow member to create, edit, delete, and reorder channels */
    constexpr Uint64 CAN_MANAGE_CHANNELS = 1ULL << 3;

    /** Allow member to manage other members permissions (future) */
    constexpr Uint64 CAN_MANAGE_PERMISSIONS = 1ULL << 4;

    /** All permissions (granted to server owner by default) */
    constexpr Uint64 ALL_PERMISSIONS = 0xFFFFFFFFFFFFFFFFULL;

    /** Helper: check if a permission bit is set */
    inline bool HasPermission(const Uint64 Permissions, const Uint64 Permission)
    {
        return (Permissions & Permission) != 0;
    }

    /** Helper: grant a permission */
    inline void Grant(Uint64& Permissions, const Uint64 Permission)
    {
        Permissions |= Permission;
    }

    /** Helper: revoke a permission */
    inline void Revoke(Uint64& Permissions, const Uint64 Permission)
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
    uint32 Position = 0;  // Display ordering (0-based). Lower = shown first.

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
    std::string Content;      // For media types this holds the verified content hash.
    Uint64 CreatedAt = 0;     // Unix timestamp as epoch nanoseconds (BIGINT UNSIGNED in DB)
    EMessageType MessageType = EMessageType::Text;

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
    bool HasPermission(const Uint64 Permission) const
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

    Uint64 GetCreatedAt() const { return CreatedAt; }
    void SetCreatedAt(const Uint64 InTime) { CreatedAt = InTime; }

    /** Channel management */
    void AddChannel(const FServerChannel& Channel);
    bool RemoveChannel(Uint64 ChannelId);
    std::shared_ptr<FServerChannel> GetChannel(Uint64 ChannelId);
    /** Returns channels sorted by Position ascending (lower = first). Cached after first sort. */
    std::vector<std::shared_ptr<FServerChannel>> GetAllChannels() const;

    /**
     * Invalidate the sorted channel cache.
     * MUST be called after external code modifies channel positions directly
     * on the shared_ptr objects returned by GetAllChannels(). This ensures
     * the next GetAllChannels() call re-sorts instead of returning stale order.
     */
    void InvalidateChannelCache();

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

    /**
     * Append a single message to the in-memory cache for a channel.
     * Used for live messages arriving in monotonic MessageId order.
     * The message is pushed to the back of the vector (oldest-first storage).
     */
    void AddMessage(const FServerMessage& Message);

    /**
     * Prepend a batch of messages to the front of the in-memory cache.
     * Used by DownloadMessagesFromDB when loading older message batches
     * from the database. The batch MUST be in ascending chronological
     * order (oldest first) before calling this method.
     */
    void PrependMessages(Uint64 ChannelId, const std::vector<FServerMessage>& Messages);

    /** Get messages before a timestamp. BeforeTimestamp=0 means no filter (get most recent). */
    std::vector<FServerMessage> GetChannelMessages(Uint64 ChannelId, Uint64 BeforeTimestamp, Uint32 Limit);

    /** Thread-safe access */
    std::shared_mutex& GetMutex() const { return ServerMutex; }

private:
    /** Server id */
    Uint64 ServerId = 0;

    /** Displayed server name */
    std::string ServerName;

    /** Owner user ID */
    Uint64 OwnerId = 0;

    /** Token for voice service / invites */
    std::string Token;

    /** Unix epoch nanoseconds timestamp of creation */
    Uint64 CreatedAt = 0;

    /** Channels: channel_id to channel */
    std::unordered_map<Uint64, std::shared_ptr<FServerChannel>> Channels;

    /** Members: user_id to member info */
    std::unordered_map<Uint64, FServerMember> Members;

    /** Messages cache: channel_id to vector of messages (appended in arrival order, newest last) */
    std::unordered_map<Uint64, std::vector<FServerMessage>> ChannelMessages;

    // --- Sorted channel cache ---
    // Populated on first GetAllChannels() call, invalidated on Add/Remove/Move/Reorder.
    // Eliminates O(n log n) sort on every read
    mutable std::vector<std::shared_ptr<FServerChannel>> CachedSortedChannels;
    mutable bool bChannelCacheValid = false;

    /** Server mutex for thread-safe access */
    mutable std::shared_mutex ServerMutex;
};
