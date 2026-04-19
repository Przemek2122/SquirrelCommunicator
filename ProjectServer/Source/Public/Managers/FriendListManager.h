// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

#include <shared_mutex>

enum class EFriendRequestStatus : uint8
{
    RequestAdded,
    RequestAlreadyExists,
};

enum class EAcceptFriendRequestStatus : uint8
{
    RequestAccepted,
    RequestNotExists,
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
    /**
     * Function used to create friend request
     * @param SendingId Who wants to add other ID
     * @param ReceivingId Who is being requested to be friend
     */
    EFriendRequestStatus SendFriendRequest(Uint64 SendingId, Uint64 ReceivingId);

    /**
     * Function used to accept friend request
     * @param AcceptingId Who is accepting existing friend-request
     * @param SendingId Who is being accpted
     */
    EAcceptFriendRequestStatus AcceptFriendRequest(Uint64 AcceptingId, Uint64 SendingId);

    /**
     * Function used to reject friend request
     * @param AcceptingId Who is rejecting existing friend-request
     * @param SendingId Who is being rejected
     */
    ERejectFriendRequestStatus RejectFriendRequest(Uint64 AcceptingId, Uint64 SendingId);

    /**
     * Function used to remove friend
     * @param RemovingId Who wants to remove friend
     * @param RemovedId Who is being removed
     */
    ERemoveFriendStatus RemoveFriend(Uint64 RemovingId, Uint64 RemovedId);

    /** Should be called on login to download user friend list and friend requests list */
    void DownloadFriendListForUserId(Uint64 UserId);

    bool HasFriendListForUserId(Uint64 UserId);

    /**
     * Function used to check if two users are friends
     * @return True if users are friends, false otherwise
     */
    bool IsFriend(Uint64 UserId, Uint64 FriendId);

    /** Get friend list array (Copy for thread safety) */
    std::vector<Uint64> GetUserFriendListArrayByUserId(Uint64 UserId);

    /** Get friend list map (Copy for thread safety) */
    std::unordered_map<Uint64, bool> GetUserFriendListMapByUserId(Uint64 UserId);

    /** Get both array and a map (Copy for thread safety) */
    FFriendList GetUserFriendListWholeByUserId(Uint64 UserId);

private:
    /**
     * Function used to fetch friends from database for user ID
     * @param UserId User ID for which friends are fetched
     */
    void DownloadFriendListFromDB(Uint64 UserId);

    /** Function used to fetch friend requests from database for user ID */
    void DownloadFriendRequestListForUserId(Uint64 UserId);

    bool DoesFriendRequestExist(Uint64 AcceptingId, Uint64 SendingId);

private:
    /** List of User IDs mapped to thier friend lists */
    std::unordered_map<Uint64, FFriendList> UserIdToFriendList;

    /** Mutex for friend list map access */
    std::shared_mutex FriendListMutex;

    /** List of User IDs mapped to thier friend requests */
    std::unordered_map<Uint64, FFriendRequestList> UserIdToFriendRequestList;

    /** Mutex for friend request list map access */
    std::shared_mutex FriendRequestListMutex;

};
