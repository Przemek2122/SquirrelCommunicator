// Created by https://www.linkedin.com/in/przemek2122/ 2026

#include "Managers/FriendListManager.h"

#include "DataBase/DataBaseConnect.h"
#include <soci/rowset.h>
#include "soci/transaction.h"
#include "Threads/ThreadsManager.h"

FFriendListManager::FFriendListManager(FThreadsManager* InThreadsManager)
    : ThreadsManager(InThreadsManager)
{
}

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

ERejectFriendRequestStatus FFriendListManager::RejectFriendRequest(const Uint64 RejectingId, const Uint64 RejectedId)
{
    const bool bFriendRequestExists = DoesFriendRequestExist(RejectingId, RejectedId);
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
                    soci::use(RejectedId), soci::use(RejectingId);

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

            if (UserIdToFriendRequestList.contains(RejectedId))
            {
                UserIdToFriendRequestList[RejectedId].FriendRequests.erase(RejectingId);
            }
        }

        return ERejectFriendRequestStatus::RequestRejected;
    }

    return ERejectFriendRequestStatus::RequestNotExists;
}

ECancelFriendRequestStatus FFriendListManager::CancelFriendRequest(const Uint64 CancelingId, const Uint64 CanceledId)
{
    const bool bFriendRequestExists = DoesFriendRequestExist(CancelingId, CanceledId);
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
                    soci::use(CancelingId), soci::use(CanceledId);

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

            if (UserIdToFriendRequestList.contains(CancelingId))
            {
                UserIdToFriendRequestList[CancelingId].FriendRequests.erase(CanceledId);
            }
        }

        return ECancelFriendRequestStatus::RequestCanceled;
    }

    return ECancelFriendRequestStatus::RequestNotExists;
}

ERemoveFriendStatus FFriendListManager::RemoveFriend(const Uint64 RemovingId, const Uint64 RemovedId)
{
    if (IsFriend(RemovingId, RemovedId))
    {
        try
        {
            FDataBaseConnect Connect;
            if (Connect.IsConnected())
            {
                soci::session& DataBaseSession = Connect.GetSession();

                long long Id1 = static_cast<long long>(RemovingId);
                long long Id2 = static_cast<long long>(RemovedId);

                // Remove whoever were first
                DataBaseSession << "DELETE FROM friends WHERE "
                                   "(id_requesting = :id1a AND id_target = :id2a) OR "
                                   "(id_requesting = :id2b AND id_target = :id1b)",
                    soci::use(Id1), soci::use(Id2), soci::use(Id2), soci::use(Id1);
            }
        }
        catch (const soci::soci_error& e)
        {
            LOG_ERROR("SOCI Error (RemoveFriend): " << e.what());
            return ERemoveFriendStatus::FriendNotExists;
        }

        {
            std::unique_lock<std::shared_mutex> WriteLock(FriendListMutex);

            if (UserIdToFriendList.contains(RemovingId))
            {
                std::vector<Uint64>& Array = UserIdToFriendList[RemovingId].FriendsArray;

                Array.erase(std::remove(Array.begin(), Array.end(), RemovedId), Array.end());
                UserIdToFriendList[RemovingId].FriendsMap.erase(RemovedId);
            }

            if (UserIdToFriendList.contains(RemovedId))
            {
                std::vector<Uint64>& Array = UserIdToFriendList[RemovedId].FriendsArray;

                Array.erase(std::remove(Array.begin(), Array.end(), RemovingId), Array.end());
                UserIdToFriendList[RemovedId].FriendsMap.erase(RemovingId);
            }
        }

        return ERemoveFriendStatus::FriendRemoved;
    }

    return ERemoveFriendStatus::FriendNotExists;
}

bool FFriendListManager::HasFriendListForUserId(const Uint64 UserId)
{
    std::shared_lock<std::shared_mutex> Lock(FriendListMutex);

    return UserIdToFriendList.contains(UserId);
}

bool FFriendListManager::HasFriendRequestListForUserId(Uint64 UserId)
{
    std::shared_lock<std::shared_mutex> Lock(FriendRequestListMutex);

    return UserIdToFriendRequestList.contains(UserId);
}

bool FFriendListManager::IsFriend(const Uint64 UserId, const Uint64 FriendId)
{
    // Make sure we do have friend list downloaded
    if (!HasFriendListForUserId(UserId))
    {
        DownloadFriendListFromDB(UserId);
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

FFriendRequestList FFriendListManager::GetUserFriendRequestListWholeByUserId(Uint64 UserId)
{
    std::shared_lock<std::shared_mutex> Lock(FriendRequestListMutex);

    if (UserIdToFriendRequestList.contains(UserId))
    {
        return UserIdToFriendRequestList[UserId];
    }

    return { };
}

std::vector<Uint64> FFriendListManager::GetFriendListInRange(const Uint64 UserId, const Uint64 Offset, const Uint64 Limit)
{
    std::vector<Uint64> OutVector;

    if (!HasFriendListForUserId(UserId))
    {
        // @TODO: Should be async for more users with proper callback
        DownloadFriendListFromDB(UserId);
    }

    if (HasFriendListForUserId(UserId))
    {
        std::shared_lock<std::shared_mutex> Lock(FriendListMutex);

        const std::vector<Uint64>& FriendsArray = UserIdToFriendList.at(UserId).FriendsArray;
        const Uint64 TotalFriends = FriendsArray.size();

        // Will it even fit?
        if (Offset < TotalFriends)
        {
            // Calculate safe end (to avoid going out of array bounds)
            const Uint64 EndIndex = std::min(Offset + Limit, TotalFriends);

            // Copy only the requested slice using iterators
            OutVector.assign(FriendsArray.begin() + Offset, FriendsArray.begin() + EndIndex);
        }
    }

    return OutVector;
}

std::vector<Uint64> FFriendListManager::GetFriendRequestListInRange(const Uint64 UserId, const Uint64 Offset, const Uint64 Limit)
{
    std::vector<Uint64> OutVector;

    if (!HasFriendRequestListForUserId(UserId))
    {
        // @TODO: Should be async for more users with proper callback
        DownloadFriendRequestListForUserId(UserId);
    }

    if (HasFriendRequestListForUserId(UserId))
    {
        std::shared_lock<std::shared_mutex> ReadLock(FriendRequestListMutex);

        if (UserIdToFriendRequestList.contains(UserId))
        {
            // Reference to the map of requests
            const std::unordered_map<Uint64, bool>& FriendRequestsMap = UserIdToFriendRequestList.at(UserId).FriendRequests;
            const Uint64 TotalRequests = FriendRequestsMap.size();

            // 3. Ensure Offset is within bounds
            if (Offset < TotalRequests)
            {
                // Move iterator to the starting offset
                std::unordered_map<Uint64, bool>::const_iterator Iterator = FriendRequestsMap.begin();
                std::advance(Iterator, Offset);

                // Calculate how many elements we can safely take
                const Uint64 ElementsToTake = std::min(Limit, TotalRequests - Offset);

                // Reserve memory for optimization
                OutVector.reserve(ElementsToTake);

                // Extract IDs (keys from the map) and push to the output vector
                for (Uint64 i = 0; i < ElementsToTake && Iterator != FriendRequestsMap.end(); ++i, ++Iterator)
                {
                    OutVector.push_back(Iterator->first);
                }
            }
        }
    }

    return OutVector;
}

void FFriendListManager::DownloadFriendListFromDB(const Uint64 UserId)
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
