// Created by https://www.linkedin.com/in/przemek2122/ 2020-2026

#include "Managers/Server.h"
#include <algorithm>

FServer::FServer()
    : ServerId(0)
    , OwnerId(0)
{
}

void FServer::InvalidateChannelCache()
{
    // Called under unique_lock by all channel mutators.
    // Also exposed publicly so ServersManager can invalidate after
    // external position modifications (MoveChannel, ReorderChannels, RenumberChannelPositions).
    bChannelCacheValid = false;
}

void FServer::AddChannel(const FServerChannel& Channel)
{
    std::unique_lock Lock(ServerMutex);
    Channels[Channel.ChannelId] = std::make_shared<FServerChannel>(Channel);
    InvalidateChannelCache();
}

bool FServer::RemoveChannel(const Uint64 ChannelId)
{
    std::unique_lock Lock(ServerMutex);
    const bool bRemoved = Channels.erase(ChannelId) > 0;
    if (bRemoved)
    {
        InvalidateChannelCache();
    }
    return bRemoved;
}

std::shared_ptr<FServerChannel> FServer::GetChannel(const Uint64 ChannelId)
{
    std::shared_lock Lock(ServerMutex);
    const auto Iter = Channels.find(ChannelId);
    if (Iter != Channels.end())
    {
        return Iter->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<FServerChannel>> FServer::GetAllChannels() const
{
    std::shared_lock Lock(ServerMutex);

    // Return cached sorted list if still valid (invalidated on Add/Remove/Move/Reorder).
    if (bChannelCacheValid)
    {
        return CachedSortedChannels;
    }

    // Build and sort
    std::vector<std::shared_ptr<FServerChannel>> Result;
    Result.reserve(Channels.size());
    for (const auto& Pair : Channels)
    {
        Result.push_back(Pair.second);
    }

    // Sort by Position ascending so channels appear in the order
    // the user arranged them (lower position = shown first).
    std::ranges::sort(Result,
        [](const std::shared_ptr<FServerChannel>& A, const std::shared_ptr<FServerChannel>& B)
        {
          return A->Position < B->Position;
        });

    // Cache for subsequent calls
    CachedSortedChannels = Result;
    bChannelCacheValid = true;

    return Result;
}

void FServer::AddMember(const FServerMember& Member)
{
    std::unique_lock Lock(ServerMutex);
    Members[Member.UserId] = Member;
}

bool FServer::RemoveMember(const Uint64 UserId)
{
    std::unique_lock Lock(ServerMutex);
    return Members.erase(UserId) > 0;
}

bool FServer::HasMember(const Uint64 UserId) const
{
    std::shared_lock Lock(ServerMutex);
    return Members.contains(UserId);
}

void FServer::UpdateMemberStatus(const Uint64 UserId, const std::string& NewStatus)
{
    std::unique_lock Lock(ServerMutex);
    const auto Iter = Members.find(UserId);
    if (Iter != Members.end())
    {
        Iter->second.Status = NewStatus;
    }
}

void FServer::UpdateMemberUserName(const Uint64 UserId, const std::string& UserName)
{
    std::unique_lock Lock(ServerMutex);
    auto Iter = Members.find(UserId);
    if (Iter != Members.end() && Iter->second.UserName.empty())
    {
        Iter->second.UserName = UserName;
    }
}

void FServer::UpdateMemberPermissions(const Uint64 UserId, const Uint64 NewPermissions)
{
    std::unique_lock Lock(ServerMutex);
    auto Iter = Members.find(UserId);
    if (Iter != Members.end())
    {
        Iter->second.Permissions = NewPermissions;
    }
}

Uint64 FServer::GetMemberPermissions(const Uint64 UserId) const
{
    std::shared_lock Lock(ServerMutex);
    auto Iter = Members.find(UserId);
    if (Iter != Members.end())
    {
        return Iter->second.Permissions;
    }
    return 0;
}

std::vector<FServerMember> FServer::GetMembers() const
{
    std::shared_lock Lock(ServerMutex);
    std::vector<FServerMember> Result;
    Result.reserve(Members.size());
    for (const auto& Pair : Members)
    {
        Result.push_back(Pair.second);
    }
    return Result;
}

size_t FServer::GetMemberCount() const
{
    std::shared_lock Lock(ServerMutex);
    return Members.size();
}

void FServer::AddMessage(const FServerMessage& Message)
{
    std::unique_lock Lock(ServerMutex);
    // Messages arrive in monotonic MessageId order (DB auto-increment).
    // push_back (append, O(1) amortized) instead of lower_bound + insert (O(n) shift).
    // GetChannelMessages reads from the back for newest-first ordering.
    ChannelMessages[Message.ChannelId].push_back(Message);
}

bool FServer::RemoveMessage(const Uint64 ChannelId, const Uint64 MessageId)
{
    std::unique_lock Lock(ServerMutex);

    const auto ChannelIter = ChannelMessages.find(ChannelId);
    if (ChannelIter == ChannelMessages.end())
    {
        return false;
    }

    std::vector<FServerMessage>& Messages = ChannelIter->second;
    const auto MessageIter = std::find_if(Messages.begin(), Messages.end(),
        [MessageId](const FServerMessage& Msg)
        {
            return Msg.MessageId == MessageId;
        });

    if (MessageIter == Messages.end())
    {
        return false;
    }

    Messages.erase(MessageIter);
    return true;
}

void FServer::PrependMessages(const Uint64 ChannelId, const std::vector<FServerMessage>& Messages)
{
    if (Messages.empty())
    {
        return;
    }

    std::unique_lock Lock(ServerMutex);
    auto& Vec = ChannelMessages[ChannelId];

    // Reserve capacity once to avoid repeated reallocations during insert
    Vec.reserve(Vec.size() + Messages.size());

    // Insert the entire batch at the front in a single operation.
    // The batch is in ascending chronological order (oldest first),
    // and it logically belongs before every message already in the vector.
    Vec.insert(Vec.begin(), Messages.begin(), Messages.end());
}

std::vector<FServerMessage> FServer::GetChannelMessages(const Uint64 ChannelId, const Uint64 BeforeTimestamp, const Uint32 Limit)
{
    std::shared_lock Lock(ServerMutex);
    const auto Iter = ChannelMessages.find(ChannelId);
    if (Iter == ChannelMessages.end())
    {
        return {};
    }

    const std::vector<FServerMessage>& Messages = Iter->second;
    std::vector<FServerMessage> Result;
    Result.reserve(std::min(static_cast<size_t>(Limit), Messages.size()));

    // Messages are stored in arrival order (oldest first, newest last).
    // Iterate in REVERSE for newest-first semantics.
    for (auto It = Messages.rbegin(); It != Messages.rend(); ++It)
    {
        const FServerMessage& Msg = *It;

        if (Result.size() >= static_cast<size_t>(Limit))
        {
            break;
        }

        if (BeforeTimestamp == 0)
        {
            // No timestamp filter: return most recent
            Result.push_back(Msg);
        }
        else if (Msg.CreatedAt < BeforeTimestamp)
        {
            Result.push_back(Msg);
        }
    }

    return Result;
}
