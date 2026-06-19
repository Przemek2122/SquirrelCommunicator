// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

#include "EngineCompat.h"

#include <shared_mutex>

enum class EFriendRequestStatus : uint8
{
    RequestAdded,
    RequestAlreadyExists,
    FriendAlreadyExists,
    SentRequestsLimitReached,
    IncomingRequestsLimitReached,
};

enum class EAcceptFriendRequestStatus : uint8
{
    RequestAccepted,
    RequestNotExists,
    FriendsLimitReached,
};

enum class ERemoveFriendStatus : uint8
{
    FriendRemoved,
    FriendNotExists,
};

enum class ERejectFriendRequestStatus : uint8
{
    RequestRejected,
    RequestNotExists,
};

enum class ECancelFriendRequestStatus : uint8
{
    RequestCanceled,
    RequestNotExists,
};

/** Structure which holds friends */
struct FFriendList
{
    /** Two containers so we can download all users fast but also search efficiently even if we reach 5000 some day */

    std::vector<Uint64> FriendsArray;
    std::unordered_map<Uint64, bool> FriendsMap;
};

/** Structure which holds friend requests */
struct FFriendRequestList
{
    std::unordered_map<Uint64, bool> FriendRequests;
};

/**
 * Manager for friends and friend-requests
 */
class FFriendListManager
{
public:
    FFriendListManager(int32 InMaxSentRequests = 25, int32 InMaxIncomingRequests = 25, int32 InMaxFriends = 200);

    EFriendRequestStatus SendFriendRequest(Uint64 SendingId, Uint64 ReceivingId);
    EAcceptFriendRequestStatus AcceptFriendRequest(Uint64 AcceptingId, Uint64 SendingId);
    ERejectFriendRequestStatus RejectFriendRequest(Uint64 RejectingId, Uint64 RejectedId);
    ECancelFriendRequestStatus CancelFriendRequest(Uint64 CancelingId, Uint64 CanceledId);
    ERemoveFriendStatus RemoveFriend(Uint64 RemovingId, Uint64 RemovedId);

    bool HasFriendListForUserId(Uint64 UserId);
    bool HasFriendRequestListForUserId(Uint64 UserId);
    bool IsFriend(Uint64 UserId, Uint64 FriendId);

    std::vector<Uint64> GetUserFriendListArrayByUserId(Uint64 UserId);
    std::unordered_map<Uint64, bool> GetUserFriendListMapByUserId(Uint64 UserId);
    FFriendList GetUserFriendListWholeByUserId(Uint64 UserId);
    FFriendRequestList GetUserFriendRequestListWholeByUserId(Uint64 UserId);
    std::vector<Uint64> GetFriendsListArrayByUserId(Uint64 UserId);
    std::vector<Uint64> GetFriendListInRange(Uint64 UserId, Uint64 Offset, Uint64 Limit);
    std::vector<Uint64> GetFriendRequestListInRange(Uint64 UserId, Uint64 Offset, Uint64 Limit);
    std::vector<Uint64> GetIncomingFriendRequestListInRange(Uint64 UserId, Uint64 Offset, Uint64 Limit);

    int32 GetMaxSentRequests() const { return MaxSentRequests; }
    int32 GetMaxIncomingRequests() const { return MaxIncomingRequests; }
    int32 GetMaxFriends() const { return MaxFriends; }

private:
    void DownloadFriendListFromDB(Uint64 UserId);
    void DownloadFriendRequestListForUserId(Uint64 UserId);
    bool DoesFriendRequestExist(Uint64 AcceptingId, Uint64 SendingId);

private:
    std::unordered_map<Uint64, FFriendList> UserIdToFriendList;
    std::shared_mutex FriendListMutex;
    std::unordered_map<Uint64, FFriendRequestList> UserIdToFriendRequestList;
    std::shared_mutex FriendRequestListMutex;



    int32 MaxSentRequests;
    int32 MaxIncomingRequests;
    int32 MaxFriends;

};
