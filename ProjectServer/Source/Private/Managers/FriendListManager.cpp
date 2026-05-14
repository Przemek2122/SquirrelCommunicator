// Created by https://www.linkedin.com/in/przemek2122/ 2026

#include "Managers/FriendListManager.h"

#include "DataBase/DataBaseConnect.h"
#include <soci/rowset.h>
#include "soci/transaction.h"
#include "Threads/ThreadsManager.h"
#include <algorithm>

FFriendListManager::FFriendListManager(FThreadsManager* InThreadsManager, int32 InMaxSentRequests, int32 InMaxIncomingRequests, int32 InMaxFriends)
    : ThreadsManager(InThreadsManager)
    , MaxSentRequests(InMaxSentRequests)
    , MaxIncomingRequests(InMaxIncomingRequests)
    , MaxFriends(InMaxFriends)
{
}

EFriendRequestStatus FFriendListManager::SendFriendRequest(const Uint64 SendingId, const Uint64 ReceivingId)
{
    if (IsFriend(SendingId, ReceivingId))
    {
        return EFriendRequestStatus::FriendAlreadyExists;
    }

    // Thread-safe check of the local cache
    {
        std::shared_lock<std::shared_mutex> Lock(FriendRequestListMutex);
        
        // Check if I already sent a request to this person
        if (UserIdToFriendRequestList.contains(SendingId))
        {
            if (UserIdToFriendRequestList[SendingId].FriendRequests.contains(ReceivingId))
            {
                return EFriendRequestStatus::RequestAlreadyExists;
            }

            // Check sent requests limit
            if (UserIdToFriendRequestList[SendingId].FriendRequests.size() >= static_cast<size_t>(MaxSentRequests))
            {
                return EFriendRequestStatus::SentRequestsLimitReached;
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
                return EFriendRequestStatus::RequestAlreadyExists;
            }

            // Double check limits in DB if needed, but for now cache is source of truth for "online" users
            // and we rely on it. In a real scenario we'd query DB for count.
            
            long long sentCount = 0;
            DataBaseSession << "SELECT count(*) FROM friend_requests WHERE id_requesting = :sid",
                soci::into(sentCount), soci::use(SendingId);
            
            if (sentCount >= MaxSentRequests)
            {
                return EFriendRequestStatus::SentRequestsLimitReached;
            }

            long long incomingCount = 0;
            DataBaseSession << "SELECT count(*) FROM friend_requests WHERE id_target = :aid",
                soci::into(incomingCount), soci::use(ReceivingId);

            if (incomingCount >= MaxIncomingRequests)
            {
                return EFriendRequestStatus::IncomingRequestsLimitReached;
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
    const bool bFriendExists = IsFriend(AcceptingId, SendingId);
    if (bFriendRequestExists && !bFriendExists)
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

                // @TODO Adding checking number of friends would be nice

                // Add friend
                DataBaseSession << "INSERT INTO friend_list (id_requesting, id_target) VALUES (:sid, :aid)",
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
                DataBaseSession << "DELETE FROM friend_list WHERE "
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
    // @TODO: Should be async for more users with proper callback
    if (!HasFriendListForUserId(UserId))
    {
        DownloadFriendListFromDB(UserId);
    }

    std::shared_lock<std::shared_mutex> Lock(FriendRequestListMutex);

    if (UserIdToFriendRequestList.contains(UserId))
    {
        return UserIdToFriendRequestList[UserId];
    }

    return { };
}

std::vector<Uint64> FFriendListManager::GetFriendsListArrayByUserId(Uint64 UserId)
{
    // @TODO: Should be async for more users with proper callback
    if (!HasFriendListForUserId(UserId))
    {
        DownloadFriendListFromDB(UserId);
    }

    std::shared_lock<std::shared_mutex> Lock(FriendListMutex);

    if (UserIdToFriendList.contains(UserId))
    {
        return UserIdToFriendList[UserId].FriendsArray;
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

std::vector<Uint64> FFriendListManager::GetIncomingFriendRequestListInRange(const Uint64 UserId, const Uint64 Offset, const Uint64 Limit)
{
    // @TODO: This function could be done better. But is good enough for now

    std::vector<Uint64> OutVector;

    // For incoming requests, we need to iterate over all users' sent requests
    // because our cache is indexed by sender.
    // In a real high-scale system, we'd have a secondary index.
    
    std::vector<Uint64> AllIncomingIds;
    {
        std::shared_lock<std::shared_mutex> Lock(FriendRequestListMutex);
        for (const auto& [SenderId, RequestList] : UserIdToFriendRequestList)
        {
            if (RequestList.FriendRequests.contains(UserId))
            {
                AllIncomingIds.push_back(SenderId);
            }
        }
    }

    // Also check DB if cache might be incomplete for "offline" senders
    // For now, let's assume DownloadFriendRequestListForUserId should have populated enough,
    // but incoming requests can come from anyone.
    
    try
    {
        FDataBaseConnect Connect;
        if (Connect.IsConnected())
        {
            soci::session& DataBaseSession = Connect.GetSession();
            
            long long Sid = 0;
            long long DBUserId = static_cast<long long>(UserId);

            soci::statement Statement =
                (DataBaseSession.prepare << "SELECT id_requesting FROM friend_requests WHERE id_target = :tid",
                 soci::into(Sid),
                 soci::use(DBUserId));

            Statement.execute();

            while (Statement.fetch())
            {
                Uint64 USid = static_cast<Uint64>(Sid);
                if (std::find(AllIncomingIds.begin(), AllIncomingIds.end(), USid) == AllIncomingIds.end())
                {
                    AllIncomingIds.push_back(USid);
                }
            }
        }
    }
    catch (const soci::soci_error& e)
    {
        LOG_ERROR("SOCI Error: " << e.what());
    }

    const Uint64 TotalRequests = AllIncomingIds.size();
    if (Offset < TotalRequests)
    {
        const Uint64 EndIndex = std::min(Offset + Limit, TotalRequests);
        OutVector.assign(AllIncomingIds.begin() + Offset, AllIncomingIds.begin() + EndIndex);
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
                "SELECT id_requesting, id_target FROM friend_list WHERE id_requesting = :id1 OR id_target = :id2",
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

                DownloadedRequests.emplace_back(Sender, Receiver);
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
    // Thread-safe check of the local cache
    {
        std::shared_lock<std::shared_mutex> Lock(FriendRequestListMutex);
        if (UserIdToFriendRequestList.contains(AcceptingId))
        {
            if (UserIdToFriendRequestList[AcceptingId].FriendRequests.contains(SendingId))
            {
                return true;
            }
        }
    }

    // If not in cache, check DB
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
                return true;
            }
        }
    }
    catch (const soci::soci_error& e)
    {
        LOG_ERROR("SOCI Error: " << e.what());
    }

    return false;
}
