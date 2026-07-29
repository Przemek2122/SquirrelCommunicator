// Created by https://www.linkedin.com/in/przemek2122/ 2020-2026

#include "Managers/Server.h"
#include <algorithm>

FServer::FServer()
    : ServerId(0)
    , OwnerId(0)
{
}

void FServer::AddChannel(const FServerChannel& Channel)
{
    std::unique_lock Lock(ServerMutex);
    Channels[Channel.ChannelId] = std::make_shared<FServerChannel>(Channel);
}

bool FServer::RemoveChannel(Uint64 ChannelId)
{
    std::unique_lock Lock(ServerMutex);
    return Channels.erase(ChannelId) > 0;
}

std::shared_ptr<FServerChannel> FServer::GetChannel(Uint64 ChannelId)
{
    std::shared_lock Lock(ServerMutex);
    auto Iter = Channels.find(ChannelId);
    if (Iter != Channels.end())
    {
        return Iter->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<FServerChannel>> FServer::GetAllChannels()
{
    std::shared_lock Lock(ServerMutex);
    std::vector<std::shared_ptr<FServerChannel>> Result;
    Result.reserve(Channels.size());
    for (const auto& Pair : Channels)
    {
        Result.push_back(Pair.second);
    }
    return Result;
}

void FServer::AddMember(const FServerMember& Member)
{
    std::unique_lock Lock(ServerMutex);
    Members[Member.UserId] = Member;
}

bool FServer::RemoveMember(Uint64 UserId)
{
    std::unique_lock Lock(ServerMutex);
    return Members.erase(UserId) > 0;
}

bool FServer::HasMember(Uint64 UserId) const
{
    std::shared_lock Lock(ServerMutex);
    return Members.find(UserId) != Members.end();
}

void FServer::UpdateMemberStatus(Uint64 UserId, const std::string& NewStatus)
{
    std::unique_lock Lock(ServerMutex);
    auto Iter = Members.find(UserId);
    if (Iter != Members.end())
    {
        Iter->second.Status = NewStatus;
    }
}

void FServer::UpdateMemberUserName(Uint64 UserId, const std::string& UserName)
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
    // Newest messages at front (index 0) - sorted by MessageId descending
    auto& Vec = ChannelMessages[Message.ChannelId];
    // Insert maintaining descending order by MessageId
    auto it = std::lower_bound(Vec.begin(), Vec.end(), Message,
        [](const FServerMessage& a, const FServerMessage& b) {
            return a.MessageId > b.MessageId;
        });
    Vec.insert(it, Message);
}

std::vector<FServerMessage> FServer::GetChannelMessages(Uint64 ChannelId, Uint64 BeforeTimestamp, int32 Limit)
{
    std::shared_lock Lock(ServerMutex);
    auto Iter = ChannelMessages.find(ChannelId);
    if (Iter == ChannelMessages.end())
    {
        return {};
    }

    const auto& Messages = Iter->second;
    std::vector<FServerMessage> Result;
    Result.reserve(std::min(static_cast<size_t>(Limit), Messages.size()));

    // Messages are stored newest-first. Iterate and collect messages before the timestamp.
    for (const auto& Msg : Messages)
    {
        if (Result.size() >= static_cast<size_t>(Limit))
        {
            break;
        }

        if (BeforeTimestamp == 0)
        {
            // No timestamp filter: return most recent
            Result.push_back(Msg);
        }
        else
        {
            // Compare timestamps: CreatedAt is stored as string epoch nanoseconds
            try
            {
                Uint64 MsgTimestamp = std::stoull(Msg.CreatedAt);
                if (MsgTimestamp < BeforeTimestamp)
                {
                    Result.push_back(Msg);
                }
            }
            catch (...)
            {
                // If parsing fails, include the message anyway
                Result.push_back(Msg);
            }
        }
    }

    return Result;
}
