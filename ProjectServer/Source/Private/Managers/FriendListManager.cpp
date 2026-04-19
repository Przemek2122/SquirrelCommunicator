// Created by https://www.linkedin.com/in/przemek2122/ 2026

#include "Managers/FriendListManager.h"

#include "DataBase/DataBaseConnect.h"
#include <soci/rowset.h>
#include <soci/row.h>
#include "soci/transaction.h"

EFriendRequestStatus FFriendListManager::SendFriendRequest(const Uint64 SendingId, const Uint64 ReceivingId)
{
    // Thread-safe check of the local cache
    {
        std::shared_lock<std::shared_mutex> Lock(FriendRequestListMutex);
        const bool bUserAlreadyExists = UserIdToFriendRequestList.contains(SendingId);
        if (bUserAlreadyExists)
        {
            if (UserIdToFriendRequestList[SendingId].FriendRequests.contains(ReceivingId))
            {
                return EFriendRequestStatus::RequestAlreadyExists;
            }
        }
    }

    // Send to DB
    try
    {
        FDataBaseConnect Connect;
        if (Connect.IsConnected())
        {
            // Get database connection session
            soci::session& DataBaseSession = Connect.GetSession();

            long long rowCount = 0;
            DataBaseSession << "SELECT count(*) FROM friend_requests "
                               "WHERE id_requesting = :sid AND id_target = :aid",
                soci::into(rowCount),
                soci::use(SendingId),
                soci::use(ReceivingId);

            if (rowCount > 0)
            {
#if DEBUG
                LOG_INFO("Friend request already exists in the database.");
#endif

                return EFriendRequestStatus::RequestAlreadyExists;
            }

            DataBaseSession << "INSERT INTO friend_requests (id_requesting, id_target) VALUES (:sid, :aid)",
                soci::use(SendingId), soci::use(ReceivingId);
        }
    }
    catch (const soci::soci_error& e)
    {
        LOG_ERROR("SOCI Error: " << e.what());
    }

    // Add to local cache
    {
        std::unique_lock<std::shared_mutex> Lock(FriendRequestListMutex);
        UserIdToFriendRequestList[SendingId].FriendRequests[ReceivingId] = true;
    }

    return EFriendRequestStatus::RequestAdded;
}

EAcceptFriendRequestStatus FFriendListManager::AcceptFriendRequest(const Uint64 AcceptingId, const Uint64 SendingId)
{
    const bool bFriendRequestExists = DoesFriendRequestExist(AcceptingId, SendingId);
    if (bFriendRequestExists)
    {
        try
        {
            FDataBaseConnect Connect;
            if (Connect.IsConnected())
            {
                // Get database connection session
                soci::session& DataBaseSession = Connect.GetSession();

                // Transaction to ensure we do not lose friend request in case of failure
                soci::transaction Tr(DataBaseSession);

                // Add friend
                DataBaseSession << "INSERT INTO friends (id_requesting, id_target) VALUES (:sid, :aid)",
                    soci::use(SendingId), soci::use(AcceptingId);

                // Remove request
                DataBaseSession << "DELETE FROM friend_requests WHERE id_requesting = :sid AND id_target = :aid",
                    soci::use(SendingId), soci::use(AcceptingId);

                Tr.commit();
            }
        }
        catch (const soci::soci_error& e)
        {
            LOG_ERROR("SOCI Error: " << e.what());
        }

        // Clear local cache
        {
            std::unique_lock<std::shared_mutex> WriteLock(FriendRequestListMutex);

            if (UserIdToFriendRequestList.contains(SendingId))
            {
                UserIdToFriendRequestList[SendingId].FriendRequests.erase(AcceptingId);
            }
        }

        // Add friends to both users
        {
            std::unique_lock<std::shared_mutex> WriteLock(FriendListMutex);

            UserIdToFriendList[AcceptingId].FriendsArray.push_back(SendingId);
            UserIdToFriendList[AcceptingId].FriendsMap[SendingId] = true;
            UserIdToFriendList[SendingId].FriendsArray.push_back(AcceptingId);
            UserIdToFriendList[SendingId].FriendsMap[AcceptingId] = true;
        }

        return EAcceptFriendRequestStatus::RequestAccepted;
    }
    else
    {
        return EAcceptFriendRequestStatus::RequestNotExists;
    }
}

ERejectFriendRequestStatus FFriendListManager::RejectFriendRequest(Uint64 AcceptingId, Uint64 SendingId)
{
    const bool bFriendRequestExists = DoesFriendRequestExist(AcceptingId, SendingId);
    if (bFriendRequestExists)
    {
        try
        {
            FDataBaseConnect Connect;
            if (Connect.IsConnected())
            {
                // Get database connection session
                soci::session& DataBaseSession = Connect.GetSession();

                // Transaction to ensure we do not lose friend-request in case of failure
                soci::transaction Tr(DataBaseSession);

                // Remove request
                DataBaseSession << "DELETE FROM friend_requests WHERE id_requesting = :sid AND id_target = :aid",
                    soci::use(SendingId), soci::use(AcceptingId);

                Tr.commit();
            }
        }
        catch (const soci::soci_error& e)
        {
            LOG_ERROR("SOCI Error: " << e.what());
        }

        // Clear local cache
        {
            std::unique_lock<std::shared_mutex> WriteLock(FriendRequestListMutex);

            if (UserIdToFriendRequestList.contains(SendingId))
            {
                UserIdToFriendRequestList[SendingId].FriendRequests.erase(AcceptingId);
            }
        }

        return ERejectFriendRequestStatus::RequestRejected;
    }
    else
    {
        return ERejectFriendRequestStatus::RequestNotExists;
    }
}

ERemoveFriendStatus FFriendListManager::RemoveFriend(const Uint64 RemovingId, const Uint64 RemovedId)
{
    if (IsFriend(RemovingId, RemovedId))
    {
        LOG_WARN("Misisng impl");


    }

    return ERemoveFriendStatus::FriendNotExists;
}

void FFriendListManager::DownloadFriendListForUserId(const Uint64 UserId)
{
    if (!HasFriendListForUserId(UserId))
    {
        DownloadFriendListFromDB(UserId);
    }
}

bool FFriendListManager::HasFriendListForUserId(const Uint64 UserId)
{
    std::shared_lock<std::shared_mutex> Lock(FriendListMutex);

    return UserIdToFriendList.contains(UserId);
}

bool FFriendListManager::IsFriend(const Uint64 UserId, const Uint64 FriendId)
{
    // Make sure we do have friend list downloaded
    if (!HasFriendListForUserId(UserId))
    {
        DownloadFriendListForUserId(UserId);
    }

    // Check cache
    std::shared_lock<std::shared_mutex> Lock(FriendListMutex);
    if (UserIdToFriendList.contains(UserId))
    {
        if (UserIdToFriendList[UserId].FriendsMap.contains(FriendId))
        {
            return true;
        }
    }
    else
    {
#if DEBUG
        LOG_ERROR("Friend list does not exist!");
#endif
    }

    return false;
}

std::vector<Uint64> FFriendListManager::GetUserFriendListArrayByUserId(const Uint64 UserId)
{
    std::shared_lock<std::shared_mutex> Lock(FriendListMutex);

    if (UserIdToFriendList.contains(UserId))
    {
        return UserIdToFriendList[UserId].FriendsArray;
    }

    return { };
}

std::unordered_map<Uint64, bool> FFriendListManager::GetUserFriendListMapByUserId(const Uint64 UserId)
{
    std::shared_lock<std::shared_mutex> Lock(FriendListMutex);

    if (UserIdToFriendList.contains(UserId))
    {
        return UserIdToFriendList[UserId].FriendsMap;
    }

    return { };
}

FFriendList FFriendListManager::GetUserFriendListWholeByUserId(const Uint64 UserId)
{
    std::shared_lock<std::shared_mutex> Lock(FriendListMutex);

    if (UserIdToFriendList.contains(UserId))
    {
        return UserIdToFriendList[UserId];
    }

    return { };
}

void FFriendListManager::DownloadFriendListFromDB(Uint64 UserId)
{
    std::vector<Uint64> DownloadedFriends;

    try
    {
        FDataBaseConnect Connect;
        if (Connect.IsConnected())
        {
            soci::session& DataBaseSession = Connect.GetSession();

            long long IdRequesting = 0;
            long long IdTarget = 0;
            long long DBUserId = static_cast<long long>(UserId);

            soci::statement st = (DataBaseSession.prepare <<
                "SELECT id_requesting, id_target FROM friends WHERE id_requesting = :id1 OR id_target = :id2",
                soci::into(IdRequesting),
                soci::into(IdTarget),
                soci::use(DBUserId),
                soci::use(DBUserId));

            st.execute();

            while (st.fetch())
            {
                Uint64 UIdRequesting = static_cast<Uint64>(IdRequesting);
                Uint64 UIdTarget = static_cast<Uint64>(IdTarget);

                if (UIdRequesting == UserId)
                {
                    DownloadedFriends.push_back(UIdTarget);
                }
                else
                {
                    DownloadedFriends.push_back(UIdRequesting);
                }
            }
        }
    }
    catch (const soci::soci_error& e)
    {
        LOG_ERROR("SOCI Error: " << e.what());
        return;
    }

    // Cache update
    {
        std::unique_lock<std::shared_mutex> WriteLock(FriendListMutex);

        // Replace list
        UserIdToFriendList[UserId].FriendsArray = std::move(DownloadedFriends);
        UserIdToFriendList[UserId].FriendsMap.clear();
        for (auto FriendId : UserIdToFriendList[UserId].FriendsArray)
        {
            UserIdToFriendList[UserId].FriendsMap[FriendId] = true;
        }
    }
}

void FFriendListManager::DownloadFriendRequestListForUserId(Uint64 UserId)
{
    std::vector<std::pair<Uint64, Uint64>> DownloadedRequests;

    try
    {
        FDataBaseConnect Connect;
        if (Connect.IsConnected())
        {
            soci::session& DataBaseSession = Connect.GetSession();

            long long IdRequesting = 0;
            long long IdTarget = 0;
            long long DBUserId = static_cast<long long>(UserId);

            soci::statement st = (DataBaseSession.prepare <<
                "SELECT id_requesting, id_target FROM friend_requests WHERE id_requesting = :id1 OR id_target = :id2",
                soci::into(IdRequesting),
                soci::into(IdTarget),
                soci::use(DBUserId),
                soci::use(DBUserId));

            st.execute();

            while (st.fetch())
            {
                const Uint64 Sender = IdRequesting;
                const Uint64 Receiver = IdTarget;

                DownloadedRequests.push_back({Sender, Receiver});
            }
        }
    }
    catch (const soci::soci_error& e)
    {
        LOG_ERROR("SOCI Error: " << e.what());
        return;
    }

    // Cache update
    {
        std::unique_lock<std::shared_mutex> WriteLock(FriendRequestListMutex);

        for (const auto& RequestPair : DownloadedRequests)
        {
            Uint64 Sender = RequestPair.first;
            Uint64 Receiver = RequestPair.second;

            UserIdToFriendRequestList[Sender].FriendRequests[Receiver] = true;
        }
    }
}

bool FFriendListManager::DoesFriendRequestExist(Uint64 AcceptingId, Uint64 SendingId)
{
    // Check if we have user downloaded
    bool bUserExists;

    // Thread-safe check of the local cache
    {
        std::shared_lock<std::shared_mutex> Lock(FriendRequestListMutex);
        bUserExists = UserIdToFriendRequestList.contains(SendingId);
    }

    bool bDoesUserHaveFriendRequest = false;

    if (!bUserExists)
    {
        // Search in DB for user
        try
        {
            FDataBaseConnect Connect;
            if (Connect.IsConnected())
            {
                // Get database connection session
                soci::session& DataBaseSession = Connect.GetSession();

                // Check if the request actually exists in DB and is still 'pending'
                long long rowCount = 0;
                DataBaseSession << "SELECT count(*) FROM friend_requests "
                                   "WHERE id_requesting = :sid AND id_target = :aid",
                    soci::use(SendingId), soci::use(AcceptingId), soci::into(rowCount);

                if (rowCount > 0)
                {
                    bDoesUserHaveFriendRequest = true;
                }
            }
        }
        catch (const soci::soci_error& e)
        {
            LOG_ERROR("SOCI Error: " << e.what());
        }
    }
    else
    {
        // We need to check again and lock and if exists, remove
        std::unique_lock<std::shared_mutex> Lock(FriendRequestListMutex);
        if (UserIdToFriendRequestList.contains(SendingId))
        {
            if (UserIdToFriendRequestList[SendingId].FriendRequests.contains(AcceptingId))
            {
                bDoesUserHaveFriendRequest = true;
                UserIdToFriendRequestList[SendingId].FriendRequests.erase(AcceptingId);
            }
        }
    }

    return bDoesUserHaveFriendRequest;
}
