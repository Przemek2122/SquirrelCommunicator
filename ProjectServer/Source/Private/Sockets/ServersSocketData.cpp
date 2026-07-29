// Created by https://www.linkedin.com/in/przemek2122/ 2026

#include "Sockets/ServersSocketData.h"

#include "ProjectEngine.h"
#include "AbuseProtection/AbuseProtection.h"
#include "Auth/UserManager.h"
#include "Auth/User.h"
#include "Managers/RoomsServiceManager.h"
#include "Managers/ServersManager.h"
#include "Managers/Server.h"
#include "Sockets/SocketManager.h"
#include "Sockets/WebSocketSessionData.h"
#include "SQRLLEncryption.h"
#include "Sockets/Socket.h"
#include "nlohmann/json.hpp"

FServersSocketData::FServersSocketData(FSocket* InSocket)
    : Socket(InSocket)
    , ProjectEngine(InSocket->GetProjectEngine())
{
    EncryptionKey = FEncryptionUtil::GenerateSecureSalt(64);
}

void FServersSocketData::PrimarySwitch(AnyWebSocket wsVariant, nlohmann::json& JsonMessage, const uWS::OpCode opCode)
{
    if (!JsonMessage.contains("type"))
    {
#if DEBUG
        LOG_ERROR("Message does not contain type");
#endif
        FSocket::EarlyExit(wsVariant, "missing type", opCode);
        return;
    }

    if (!JsonMessage.contains("data"))
    {
#if DEBUG
        LOG_ERROR("Message does not contain data");
#endif
        FSocket::EarlyExit(wsVariant, "missing data", opCode);
        return;
    }

    const std::string& SocketMessageServersType = JsonMessage["type"];
    const nlohmann::basic_json<>& DataJSON = JsonMessage["data"];
    const ESocketMessageServersType Type = StringToSocketMessageServersType(SocketMessageServersType);

    switch (Type)
    {
        case ESocketMessageServersType::CreateRoom:
        {
            if (DataJSON.contains("room_name"))
            {
                CreateRoom(wsVariant, opCode, DataJSON["room_name"].get<std::string>());
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing room_name", opCode);
            }
            break;
        }

        case ESocketMessageServersType::JoinRoom:
        {
            if (DataJSON.contains("room_id"))
            {
                const Uint64 RoomId = std::stoull(DataJSON["room_id"].get<std::string>());
                JoinRoom(wsVariant, opCode, RoomId);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing room_id", opCode);
            }
            break;
        }

        case ESocketMessageServersType::LeaveRoom:
        {
            if (DataJSON.contains("room_id"))
            {
                const Uint64 RoomId = std::stoull(DataJSON["room_id"].get<std::string>());
                LeaveRoom(wsVariant, opCode, RoomId);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing room_id", opCode);
            }
            break;
        }

        case ESocketMessageServersType::RoomMessage:
        {
            if (DataJSON.contains("room_id") && DataJSON.contains("channel_id") && DataJSON.contains("content"))
            {
                const Uint64 RoomId = std::stoull(DataJSON["room_id"].get<std::string>());
                const Uint64 ChannelId = std::stoull(DataJSON["channel_id"].get<std::string>());
                const std::string Content = DataJSON["content"].get<std::string>();
                RoomMessage(wsVariant, opCode, RoomId, ChannelId, Content);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing room_message fields", opCode);
            }
            break;
        }

        case ESocketMessageServersType::CreateChannel:
        {
            if (DataJSON.contains("room_id") && DataJSON.contains("channel_name") && DataJSON.contains("channel_type"))
            {
                const Uint64 RoomId = std::stoull(DataJSON["room_id"].get<std::string>());
                const std::string ChannelName = DataJSON["channel_name"].get<std::string>();
                const std::string ChannelType = DataJSON["channel_type"].get<std::string>();
                CreateChannel(wsVariant, opCode, RoomId, ChannelName, ChannelType);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing create_channel fields", opCode);
            }
            break;
        }

        case ESocketMessageServersType::ServerInvite:
        {
            if (DataJSON.contains("room_id") && DataJSON.contains("user_id"))
            {
                const Uint64 RoomId = std::stoull(DataJSON["room_id"].get<std::string>());
                const Uint64 TargetUserId = std::stoull(DataJSON["user_id"].get<std::string>());
                ServerInvite(wsVariant, opCode, RoomId, TargetUserId);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing server_invite fields", opCode);
            }
            break;
        }

        case ESocketMessageServersType::RoomJoinVoice:
        {
            if (DataJSON.contains("room_id") && DataJSON.contains("channel_id"))
            {
                const Uint64 RoomId = std::stoull(DataJSON["room_id"].get<std::string>());
                const Uint64 ChannelId = std::stoull(DataJSON["channel_id"].get<std::string>());
                RoomJoinVoice(wsVariant, opCode, RoomId, ChannelId);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing server_join_voice fields", opCode);
            }
            break;
        }

        case ESocketMessageServersType::RoomLeaveVoice:
        {
            if (DataJSON.contains("room_id") && DataJSON.contains("channel_id"))
            {
                const Uint64 RoomId = std::stoull(DataJSON["room_id"].get<std::string>());
                const Uint64 ChannelId = std::stoull(DataJSON["channel_id"].get<std::string>());
                RoomLeaveVoice(wsVariant, opCode, RoomId, ChannelId);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing server_leave_voice fields", opCode);
            }
            break;
        }

        // ========== NEW: WebSocket-based replacements for REST endpoints ==========

        case ESocketMessageServersType::GetServerList:
        {
            HandleGetServerList(wsVariant, opCode);
            break;
        }

        case ESocketMessageServersType::GetServerMessages:
        {
            if (DataJSON.contains("room_id") && DataJSON.contains("channel_id"))
            {
                const Uint64 RoomId = std::stoull(DataJSON["room_id"].get<std::string>());
                const Uint64 ChannelId = std::stoull(DataJSON["channel_id"].get<std::string>());
                Uint64 Before = 0;
                int32 Limit = 50;

                if (DataJSON.contains("before"))
                    Before = std::stoull(DataJSON["before"].get<std::string>());
                if (DataJSON.contains("limit"))
                    Limit = std::stoi(DataJSON["limit"].get<std::string>());

                if (Limit <= 0 || Limit > 100) Limit = 50;

                HandleGetServerMessages(wsVariant, opCode, RoomId, ChannelId, Before, Limit);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing room_id or channel_id", opCode);
            }
            break;
        }

        case ESocketMessageServersType::ServerCreateInvite:
        {
            if (DataJSON.contains("room_id"))
            {
                const Uint64 RoomId = std::stoull(DataJSON["room_id"].get<std::string>());

                // Optional: max uses and expiration time (configured by API caller)
                Uint32 MaxUses = 0;
                Uint32 ExpiresInSeconds = 0;

                if (DataJSON.contains("max_uses"))
                    MaxUses = DataJSON["max_uses"].get<Uint32>();
                if (DataJSON.contains("expires_in_seconds"))
                    ExpiresInSeconds = DataJSON["expires_in_seconds"].get<Uint32>();

                HandleServerCreateInvite(wsVariant, opCode, RoomId, MaxUses, ExpiresInSeconds);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing room_id", opCode);
            }
            break;
        }

        case ESocketMessageServersType::ServerJoinInvite:
        {
            if (DataJSON.contains("invite_code"))
            {
                HandleServerJoinInvite(wsVariant, opCode, DataJSON["invite_code"].get<std::string>());
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing invite_code", opCode);
            }
            break;
        }

        // ========== NEW: Permission management ==========

        case ESocketMessageServersType::ServerUpdateMemberPermissions:
        {
            if (DataJSON.contains("room_id") && DataJSON.contains("target_user_id") && DataJSON.contains("permissions"))
            {
                const Uint64 RoomId = std::stoull(DataJSON["room_id"].get<std::string>());
                const Uint64 TargetUserId = std::stoull(DataJSON["target_user_id"].get<std::string>());
                const Uint64 NewPermissions = std::stoull(DataJSON["permissions"].get<std::string>());

                HandleUpdateMemberPermissions(wsVariant, opCode, RoomId, TargetUserId, NewPermissions);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing field in server_update_member_permissions. Required: room_id, target_user_id, permissions.", opCode);
            }
            break;
        }

        // ========== NEW: Invite management ==========

        case ESocketMessageServersType::ServerDeleteInvite:
        {
            if (DataJSON.contains("room_id") && DataJSON.contains("invite_code"))
            {
                const Uint64 RoomId = std::stoull(DataJSON["room_id"].get<std::string>());
                const std::string InviteCode = DataJSON["invite_code"].get<std::string>();
                HandleServerDeleteInvite(wsVariant, opCode, RoomId, InviteCode);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing room_id or invite_code", opCode);
            }
            break;
        }

        case ESocketMessageServersType::ServerListInvites:
        {
            if (DataJSON.contains("room_id"))
            {
                const Uint64 RoomId = std::stoull(DataJSON["room_id"].get<std::string>());
                int32 Start = 0;
                int32 Count = 50;

                if (DataJSON.contains("start"))
                    Start = DataJSON["start"].get<int32>();
                if (DataJSON.contains("count"))
                    Count = DataJSON["count"].get<int32>();

                // Clamp pagination params at socket layer so response values are accurate.
                // Consistent with FServersManager::ListInvites: clamp Count to 200 max, default 50.
                if (Start < 0) Start = 0;
                if (Count <= 0) Count = 50;
                else if (Count > 200) Count = 200;

                HandleServerListInvites(wsVariant, opCode, RoomId, Start, Count);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing room_id", opCode);
            }
            break;
        }

        case ESocketMessageServersType::Unknown:
        case ESocketMessageServersType::Error:
        default:
        {
            nlohmann::json ErrorJson;
            ErrorJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::Error);
            ErrorJson["message"] = "Unknown message type";

            std::visit([&](auto* ws)
            {
                ws->send(ErrorJson.dump(), opCode);
            }, wsVariant);

            break;
        }
    }
}

void FServersSocketData::CreateRoom(AnyWebSocket wsVariant, uWS::OpCode opCode, const std::string& RoomName)
{
    if (RoomName.empty())
    {
        FSocket::EarlyExit(wsVariant, "empty room_name", opCode);
        return;
    }

    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    // Rate-limit check
    const std::string ClientIP = GetRemoteIP(wsVariant);
    FAbuseProtection* AbuseProtection = ProjectEngine->GetAbuseProtection();

    if (!ClientIP.empty() && !AbuseProtection->CanAddressRequestCreateServer(ClientIP))
    {
        FSocket::EarlyExit(wsVariant, "service abuse", opCode);
        return;
    }
    AbuseProtection->AddCreateServerAttempt(ClientIP);

    // Create the server
    FServersManager* ServersManager = ProjectEngine->GetServersManager();
    const Uint64 NewServerId = ServersManager->AddServer(RoomName, CurrentUserId);

    if (NewServerId == 0)
    {
        FSocket::EarlyExit(wsVariant, "failed to create room", opCode);
        return;
    }

    // Get the created server to build the response
    auto Server = ServersManager->GetServerById(NewServerId);
    if (!Server)
    {
        FSocket::EarlyExit(wsVariant, "server created but not found", opCode);
        return;
    }

    // Build response with room data
    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::RoomCreated);
    ResponseJson["data"] = BuildRoomDataJson(NewServerId);

    // Send confirmation to the creator
    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    LOG_INFO("Room created: '" << RoomName << "' (ID: " << NewServerId << ") by user " << CurrentUserId);
}

void FServersSocketData::JoinRoom(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 RoomId)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    // Security: JoinRoom is only for re-joining servers the user is already a member of.
    // New members must use ServerJoinInvite with a valid invite code.
    if (!ServersManager->IsUserInServer(RoomId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this room. Use server_join_invite with a valid invite code.", opCode);
        return;
    }

    const std::string UserName = GetUserName(CurrentUserId);

    // Send full room data to the re-joining user
    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::RoomCreated);
    ResponseJson["data"] = BuildRoomDataJson(RoomId);

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    LOG_INFO("User " << CurrentUserId << " re-joined room " << RoomId);
}

void FServersSocketData::LeaveRoom(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 RoomId)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    if (!ServersManager->RemoveUserFromServer(RoomId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "failed to leave room", opCode);
        return;
    }

    // Broadcast to remaining members
    nlohmann::json BroadcastJson;
    BroadcastJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::RoomUserLeft);
    BroadcastJson["data"]["room_id"] = RoomId;
    BroadcastJson["data"]["user_id"] = CurrentUserId;

    BroadcastToServerMembers(RoomId, BroadcastJson);

    // Confirm to the leaving user
    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::LeaveRoom);
    ResponseJson["data"]["room_id"] = RoomId;
    ResponseJson["data"]["status"] = "left";

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    LOG_INFO("User " << CurrentUserId << " left room " << RoomId);
}

void FServersSocketData::RoomMessage(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 RoomId, Uint64 ChannelId, const std::string& Content)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    // Verify user is a member
    if (!ServersManager->IsUserInServer(RoomId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this room", opCode);
        return;
    }

    // Check max message size
    if (Content.size() > ProjectEngine->GetBackendSettings()->GetMaxMessageSize())
    {
        FSocket::EarlyExit(wsVariant, "message too large", opCode);
        return;
    }

    const std::string UserName = GetUserName(CurrentUserId);

    // Persist the message
    const Uint64 MessageId = ServersManager->AddMessage(RoomId, ChannelId, CurrentUserId, UserName, Content);
    if (MessageId == 0)
    {
        FSocket::EarlyExit(wsVariant, "failed to save message", opCode);
        return;
    }

    // Broadcast to all room members (including sender for consistency with frontend)
    nlohmann::json BroadcastJson;
    BroadcastJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::RoomMessage);
    BroadcastJson["data"]["room_id"] = RoomId;
    BroadcastJson["data"]["channel_id"] = ChannelId;
    BroadcastJson["data"]["message_id"] = MessageId;
    BroadcastJson["data"]["sender_id"] = CurrentUserId;
    BroadcastJson["data"]["sender_name"] = UserName;
    BroadcastJson["data"]["content"] = Content;
    BroadcastJson["data"]["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();

    BroadcastToServerMembers(RoomId, BroadcastJson);
}

void FServersSocketData::CreateChannel(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 RoomId, const std::string& ChannelName, const std::string& ChannelType)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    if (!ServersManager->IsUserInServer(RoomId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this room", opCode);
        return;
    }

    const EServerChannelType Type = (ChannelType == "voice") ? EServerChannelType::Voice : EServerChannelType::Text;
    const Uint64 NewChannelId = ServersManager->AddChannel(RoomId, ChannelName, Type);

    if (NewChannelId == 0)
    {
        FSocket::EarlyExit(wsVariant, "failed to create channel", opCode);
        return;
    }

    // Broadcast to all room members
    nlohmann::json BroadcastJson;
    BroadcastJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::RoomChannelCreated);
    BroadcastJson["data"]["room_id"] = RoomId;
    BroadcastJson["data"]["channel_id"] = NewChannelId;
    BroadcastJson["data"]["channel_name"] = ChannelName;
    BroadcastJson["data"]["channel_type"] = ChannelType;

    BroadcastToServerMembers(RoomId, BroadcastJson);

    LOG_INFO("Channel '" << ChannelName << "' created in room " << RoomId);
}

void FServersSocketData::ServerInvite(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 RoomId, Uint64 TargetUserId)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    if (!ServersManager->IsUserInServer(RoomId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this room", opCode);
        return;
    }

    auto Server = ServersManager->GetServerById(RoomId);
    if (!Server)
    {
        FSocket::EarlyExit(wsVariant, "room not found", opCode);
        return;
    }

    const std::string UserName = GetUserName(CurrentUserId);

    // Send invite notification to the target user
    nlohmann::json InviteJson;
    InviteJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerInvite);
    InviteJson["data"]["room_id"] = RoomId;
    InviteJson["data"]["room_name"] = Server->GetServerName();
    InviteJson["data"]["inviter_id"] = CurrentUserId;
    InviteJson["data"]["inviter_name"] = UserName;

    SendToUser(TargetUserId, InviteJson);

    // Confirm to the inviter
    nlohmann::json ConfirmJson;
    ConfirmJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerInvite);
    ConfirmJson["data"]["status"] = "sent";
    ConfirmJson["data"]["room_id"] = RoomId;
    ConfirmJson["data"]["user_id"] = TargetUserId;

    std::visit([&](auto* ws)
    {
        ws->send(ConfirmJson.dump(), opCode);
    }, wsVariant);

    LOG_INFO("User " << CurrentUserId << " invited user " << TargetUserId << " to room " << RoomId);
}

void FServersSocketData::RoomJoinVoice(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 RoomId, Uint64 ChannelId)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    if (!ServersManager->IsUserInServer(RoomId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this room", opCode);
        return;
    }

    // Verify channel exists and is voice type
    std::shared_ptr<FServer> Server = ServersManager->GetServerById(RoomId);
    if (!Server)
    {
        FSocket::EarlyExit(wsVariant, "room not found", opCode);
        return;
    }

    std::shared_ptr<FServerChannel> Channel = Server->GetChannel(ChannelId);
    if (!Channel || Channel->ChannelType != EServerChannelType::Voice)
    {
        FSocket::EarlyExit(wsVariant, "invalid voice channel", opCode);
        return;
    }

    ServersManager->JoinVoiceChannel(RoomId, ChannelId, CurrentUserId);

    // Also create/check the Go voice service room
    const std::string VoiceRoomName = "Server_" + std::to_string(RoomId) + "_" + std::to_string(ChannelId);

    const ERoomExistenceStatus CheckResult = ProjectEngine->GetRoomsManager()->CheckRoom(VoiceRoomName);
    if (CheckResult == ERoomExistenceStatus::NotExists)
    {
        ProjectEngine->GetRoomsManager()->CreateRoom(VoiceRoomName);
    }

    const std::string RoomToken = ProjectEngine->GetRoomsManager()->GetRoomToken(VoiceRoomName);
    const std::string UserName = GetUserName(CurrentUserId);

    // Send data_stream_channel info back so frontend can connect to Go service
    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::RoomJoinVoice);
    ResponseJson["data"]["name"] = VoiceRoomName;
    ResponseJson["data"]["token"] = RoomToken;
    ResponseJson["data"]["user_name"] = UserName;

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    // Broadcast voice join to room members
    nlohmann::json BroadcastJson;
    BroadcastJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::RoomUserVoiceJoin);
    BroadcastJson["data"]["room_id"] = RoomId;
    BroadcastJson["data"]["channel_id"] = ChannelId;
    BroadcastJson["data"]["user_id"] = CurrentUserId;
    BroadcastJson["data"]["user_name"] = UserName;

    BroadcastToServerMembers(RoomId, BroadcastJson, CurrentUserId);

    LOG_INFO("User " << CurrentUserId << " joined voice channel " << ChannelId << " in room " << RoomId);
}

void FServersSocketData::RoomLeaveVoice(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 RoomId, Uint64 ChannelId)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    ServersManager->LeaveVoiceChannel(RoomId, ChannelId, CurrentUserId);

    const std::string UserName = GetUserName(CurrentUserId);

    // Confirm to the user
    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::RoomLeaveVoice);
    ResponseJson["data"]["status"] = "disconnected";

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    // Broadcast to room members
    nlohmann::json BroadcastJson;
    BroadcastJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::RoomUserVoiceLeave);
    BroadcastJson["data"]["room_id"] = RoomId;
    BroadcastJson["data"]["channel_id"] = ChannelId;
    BroadcastJson["data"]["user_id"] = CurrentUserId;
    BroadcastJson["data"]["user_name"] = UserName;

    BroadcastToServerMembers(RoomId, BroadcastJson, CurrentUserId);

    LOG_INFO("User " << CurrentUserId << " left voice channel " << ChannelId << " in room " << RoomId);
}

// ========== NEW: WebSocket handlers replacing REST endpoints ==========

void FServersSocketData::HandleGetServerList(AnyWebSocket wsVariant, uWS::OpCode opCode)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();
    const std::vector<std::shared_ptr<FServer>> Servers = ServersManager->GetUserServers(CurrentUserId);

    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerList);

    nlohmann::json RoomsArray = nlohmann::json::array();

    for (const auto& Server : Servers)
    {
        RoomsArray.push_back(BuildRoomDataJson(Server->GetServerId()));
    }

    ResponseJson["data"]["rooms"] = RoomsArray;

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);
}

void FServersSocketData::HandleGetServerMessages(AnyWebSocket wsVariant, uWS::OpCode opCode, const Uint64 RoomId, const Uint64 ChannelId, const Uint64 Before, const Uint32 Limit)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    // Verify membership
    if (!ServersManager->IsUserInServer(RoomId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this room", opCode);
        return;
    }

    // Use timestamp-based pagination
    const std::vector<FServerMessage>& Messages = ServersManager->GetChannelMessages(RoomId, ChannelId, Before, Limit);

    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerMessages);

    nlohmann::json MessagesArray = nlohmann::json::array();

    for (const auto& Msg : Messages)
    {
        nlohmann::json MsgJson;
        MsgJson["message_id"] = std::to_string(Msg.MessageId);
        MsgJson["channel_id"] = std::to_string(Msg.ChannelId);
        MsgJson["sender_id"] = std::to_string(Msg.SenderId);
        MsgJson["sender_name"] = Msg.SenderName;
        MsgJson["content"] = Msg.Content;
        MsgJson["timestamp"] = Msg.CreatedAt;
        MessagesArray.push_back(MsgJson);
    }

    ResponseJson["data"]["messages"] = MessagesArray;
    ResponseJson["data"]["has_more"] = (static_cast<int32>(Messages.size()) >= Limit);

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);
}

void FServersSocketData::HandleServerCreateInvite(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                                    const Uint64 RoomId, const Uint32 MaxUses, const Uint32 ExpiresInSeconds)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    if (!ServersManager->IsUserInServer(RoomId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this room", opCode);
        return;
    }

    // Check permission: user must have CAN_CREATE_INVITES or be owner
    if (!ServersManager->UserHasPermission(RoomId, CurrentUserId, EServerPermission::CAN_CREATE_INVITES))
    {
        FSocket::EarlyExit(wsVariant, "permission denied: you lack CAN_CREATE_INVITES permission", opCode);
        return;
    }

    const std::string InviteCode = ServersManager->CreateInvite(RoomId, CurrentUserId, MaxUses, ExpiresInSeconds);
    if (InviteCode.empty())
    {
        FSocket::EarlyExit(wsVariant, "failed to create invite", opCode);
        return;
    }

    // Resolve actual defaults used (for response)
    const FBackendSettings* Settings = ProjectEngine->GetBackendSettings();
    const int32 ActualMaxUses = (MaxUses > 0) ? MaxUses : Settings->GetInviteDefaultMaxUses();
    int32 ActualExpiresInSeconds = (ExpiresInSeconds > 0) ? ExpiresInSeconds : Settings->GetInviteDefaultExpiresInSeconds();
    if (ActualExpiresInSeconds > Settings->GetInviteMaxExpiresInSeconds())
        ActualExpiresInSeconds = Settings->GetInviteMaxExpiresInSeconds();

    // Compute actual expires_at as Unix timestamp for frontend
    const auto ExpiresTime = std::chrono::system_clock::now() + std::chrono::seconds(ActualExpiresInSeconds);
    const Uint64 ExpiresAtUnix = std::chrono::duration_cast<std::chrono::seconds>(
        ExpiresTime.time_since_epoch()).count();

    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerInviteCreated);
    ResponseJson["data"]["invite_code"] = InviteCode;
    ResponseJson["data"]["invite_url"] = "https://comm.sqrll.net/invite/" + InviteCode;
    ResponseJson["data"]["max_uses"] = ActualMaxUses;
    ResponseJson["data"]["expires_at"] = ExpiresAtUnix;         // Unix timestamp (seconds)
    ResponseJson["data"]["expires_in_seconds"] = ActualExpiresInSeconds;

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    LOG_INFO("Invite code created for room " << RoomId << " by user " << CurrentUserId
             << " (maxUses=" << ActualMaxUses << ", expiresIn=" << ActualExpiresInSeconds << "s)");
}

void FServersSocketData::HandleServerJoinInvite(AnyWebSocket wsVariant, uWS::OpCode opCode, const std::string& InviteCode)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    const std::string UserName = GetUserName(CurrentUserId);
    const std::string ClientIP = GetRemoteIP(wsVariant);

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    // Pass IP for invite abuse protection. OutError gives us a specific error code.
    std::string ErrorCode;
    auto Server = ServersManager->JoinViaInvite(InviteCode, CurrentUserId, UserName, ClientIP, &ErrorCode);

    if (!Server)
    {
        if (ErrorCode == "abuse_ban")
        {
            // IP is banned for too many failed invite attempts.
            // Send a specific error so frontend can show "try again in X minutes".
            FSocket::EarlyExit(wsVariant, "abuse ban: too many failed invite attempts. Try again later.", opCode);
        }
        else
        {
            FSocket::EarlyExit(wsVariant, "invalid or expired invite code", opCode);
        }
        return;
    }

    // Send full room data to the joining user
    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerJoined);
    ResponseJson["data"] = BuildRoomDataJson(Server->GetServerId());

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    // Broadcast to room members that a new user joined
    nlohmann::json BroadcastJson;
    BroadcastJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::RoomUserJoined);
    BroadcastJson["data"]["room_id"] = Server->GetServerId();
    BroadcastJson["data"]["user_id"] = CurrentUserId;
    BroadcastJson["data"]["user_name"] = UserName;

    BroadcastToServerMembers(Server->GetServerId(), BroadcastJson, CurrentUserId);

    LOG_INFO("User " << CurrentUserId << " joined via invite code to server " << Server->GetServerId());
}

// ========== NEW: Permission Management ==========

void FServersSocketData::HandleUpdateMemberPermissions(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                                         Uint64 RoomId, Uint64 TargetUserId, Uint64 NewPermissions)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    // Only the server owner can manage permissions (future: CAN_MANAGE_PERMISSIONS permission)
    if (!IsServerOwner(RoomId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "permission denied: only the server owner can manage member permissions", opCode);
        return;
    }

    // Cannot modify owner's permissions
    const std::shared_ptr<FServer> Server = ServersManager->GetServerById(RoomId);
    if (Server && Server->GetOwnerId() == TargetUserId)
    {
        FSocket::EarlyExit(wsVariant, "cannot modify the server owners permissions", opCode);
        return;
    }

    if (!ServersManager->IsUserInServer(RoomId, TargetUserId))
    {
        FSocket::EarlyExit(wsVariant, "target user is not a member of this room", opCode);
        return;
    }

    if (!ServersManager->UpdateMemberPermissions(RoomId, TargetUserId, NewPermissions))
    {
        FSocket::EarlyExit(wsVariant, "failed to update member permissions", opCode);
        return;
    }

    // Send confirmation to the requester
    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerMemberPermissionsUpdated);
    ResponseJson["data"]["room_id"] = RoomId;
    ResponseJson["data"]["user_id"] = TargetUserId;
    ResponseJson["data"]["permissions"] = NewPermissions;

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    // Also broadcast the updated room data to all members (so permissions reflect in member list)
    nlohmann::json BroadcastJson;
    BroadcastJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerMemberPermissionsUpdated);
    BroadcastJson["data"]["room_id"] = RoomId;
    BroadcastJson["data"]["user_id"] = TargetUserId;
    BroadcastJson["data"]["permissions"] = NewPermissions;

    BroadcastToServerMembers(RoomId, BroadcastJson);

    LOG_INFO("Permissions updated for user " << TargetUserId << " in room " << RoomId
             << " by owner " << CurrentUserId << " to " << NewPermissions);
}

// ========== NEW: Invite Management ==========

void FServersSocketData::HandleServerDeleteInvite(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                                    Uint64 RoomId, const std::string& InviteCode)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    if (!ServersManager->IsUserInServer(RoomId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this room", opCode);
        return;
    }

    // Check permission: user must have CAN_CREATE_INVITES or be owner
    if (!ServersManager->UserHasPermission(RoomId, CurrentUserId, EServerPermission::CAN_CREATE_INVITES))
    {
        FSocket::EarlyExit(wsVariant, "permission denied: you lack CAN_CREATE_INVITES permission", opCode);
        return;
    }

    if (!ServersManager->DeleteInvite(RoomId, InviteCode, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "invite not found or already deleted", opCode);
        return;
    }

    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerInviteDeleted);
    ResponseJson["data"]["room_id"] = RoomId;
    ResponseJson["data"]["invite_code"] = InviteCode;

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    LOG_INFO("Invite " << InviteCode << " deleted from room " << RoomId
             << " by user " << CurrentUserId);
}

void FServersSocketData::HandleServerListInvites(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                                   Uint64 RoomId, int32 Start, int32 Count)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    if (!ServersManager->IsUserInServer(RoomId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this room", opCode);
        return;
    }

    // Check permission: user must have CAN_CREATE_INVITES or be owner
    if (!ServersManager->UserHasPermission(RoomId, CurrentUserId, EServerPermission::CAN_CREATE_INVITES))
    {
        FSocket::EarlyExit(wsVariant, "permission denied: you lack CAN_CREATE_INVITES permission", opCode);
        return;
    }

    int32 Total = 0;
    auto Invites = ServersManager->ListInvites(RoomId, CurrentUserId, Start, Count, &Total);

    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerInvitesList);
    ResponseJson["data"]["room_id"] = RoomId;
    ResponseJson["data"]["start"] = Start;
    ResponseJson["data"]["count"] = static_cast<int32>(Invites.size());
    ResponseJson["data"]["total"] = Total;

    nlohmann::json InvitesArray = nlohmann::json::array();
    for (const auto& Inv : Invites)
    {
        nlohmann::json InvJson;
        InvJson["invite_code"] = Inv.InviteCode;
        InvJson["created_by"] = std::to_string(Inv.CreatedBy);
        InvJson["max_uses"] = Inv.MaxUses;
        InvJson["current_uses"] = Inv.CurrentUses;
        InvJson["remaining_uses"] = Inv.RemainingUses();
        InvJson["created_at"] = Inv.CreatedAt;
        InvJson["expires_at"] = Inv.ExpiresAt;
        InvitesArray.push_back(InvJson);
    }

    ResponseJson["data"]["invites"] = InvitesArray;

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    LOG_INFO("Listed " << Invites.size() << " invites for room " << RoomId
             << " (total " << Total << ", page start=" << Start << ") by user " << CurrentUserId);
}

// ========== Public: Member Status Broadcast ==========

void FServersSocketData::BroadcastMemberStatus(Uint64 UserId, const std::string& UserName, EUserStatus NewStatus)
{
    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    // Get all servers this user is a member of (lightweight: just IDs from DB)
    const auto ServerIds = ServersManager->GetUserServerIds(UserId);

    const std::string StatusStr = UserStatusToString(NewStatus);

    for (Uint64 ServerId : ServerIds)
    {
        // Update the in-memory member status if server is cached
        auto Server = ServersManager->GetServerById(ServerId);
        if (Server)
        {
            Server->UpdateMemberStatus(UserId, StatusStr);
        }

        // Build broadcast message
        nlohmann::json BroadcastJson;
        BroadcastJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::RoomMemberStatus);
        BroadcastJson["data"]["room_id"] = ServerId;
        BroadcastJson["data"]["user_id"] = UserId;
        BroadcastJson["data"]["user_name"] = UserName;
        BroadcastJson["data"]["status"] = StatusStr;

        // Broadcast to all members of this server (except the user themselves)
        BroadcastToServerMembers(ServerId, BroadcastJson, UserId);
    }

    LOG_INFO("Broadcasted status change for user " << UserId << " to " << StatusStr
             << " across " << ServerIds.size() << " servers");
}

// ========== Public: Voice Channel Auto-Cleanup ==========

void FServersSocketData::CleanupUserVoiceChannels(Uint64 UserId, const std::string& UserName)
{
    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    // Find all voice channels this user is currently connected to
    const auto VoiceChannels = ServersManager->GetUserVoiceChannels(UserId);

    if (VoiceChannels.empty())
    {
        return;
    }

    for (const auto& [ServerId, ChannelId] : VoiceChannels)
    {
        // Remove user from the voice channel in-memory state
        ServersManager->LeaveVoiceChannel(ServerId, ChannelId, UserId);

        // Broadcast to other room members that user left voice
        nlohmann::json BroadcastJson;
        BroadcastJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::RoomUserVoiceLeave);
        BroadcastJson["data"]["room_id"] = ServerId;
        BroadcastJson["data"]["channel_id"] = ChannelId;
        BroadcastJson["data"]["user_id"] = UserId;
        BroadcastJson["data"]["user_name"] = UserName;

        BroadcastToServerMembers(ServerId, BroadcastJson, UserId);

        LOG_INFO("Auto-cleaned voice: User " << UserId << " removed from channel "
                 << ChannelId << " in server " << ServerId);
    }
}

// ========== Private Helpers ==========

void FServersSocketData::BroadcastToServerMembers(Uint64 ServerId, const nlohmann::json& JsonMessage, Uint64 ExcludeUserId)
{
    FServersManager* ServersManager = ProjectEngine->GetServersManager();
    auto Server = ServersManager->GetServerById(ServerId);
    if (!Server)
    {
        return;
    }

    auto Members = Server->GetMembers();
    for (const auto& Member : Members)
    {
        if (Member.UserId == ExcludeUserId)
        {
            continue;
        }

        SendToUser(Member.UserId, JsonMessage);
    }
}

void FServersSocketData::SendToUser(Uint64 UserId, const nlohmann::json& JsonMessage)
{
    FSocketManager* SocketMgr = ProjectEngine->GetSocketManager();
    FUserManager* UserMgr = ProjectEngine->GetUserManager();

    auto User = UserMgr->GetUserById(UserId);
    if (!User || User->GetUserStatus() != EUserStatus::Online)
    {
        return; // User is offline, skip
    }

    const std::string Serialized = JsonMessage.dump();

    FFunctorLambda<void, void*> SocketAccessFunctor = [Serialized, UserId](void* targetWs)
    {
        auto* WebSocket = static_cast<uWS::WebSocket<false, true, FWebSocketSessionData>*>(targetWs);
        const std::string UserTopic = FSocket::GenerateUserTopic(UserId);
        if (WebSocket->isSubscribed(UserTopic))
        {
            WebSocket->send(Serialized, uWS::OpCode::TEXT);
        }
    };

    SocketMgr->EnqueueTaskForUserAtSocket(User->GetSocketId(), UserId, SocketAccessFunctor);
}

Uint64 FServersSocketData::GetUserIdFromWS(AnyWebSocket wsVariant)
{
    Uint64 UserId = 0;
    std::visit([&](auto* ws)
    {
        FWebSocketSessionData* SessionData = ws->getUserData();
        if (SessionData != nullptr)
        {
            UserId = SessionData->UserId;
        }
    }, wsVariant);
    return UserId;
}

std::string FServersSocketData::GetUserName(const Uint64 UserId)
{
    const std::shared_ptr<FUser> User = ProjectEngine->GetUserManager()->GetUserById(UserId);
    if (User)
    {
        return User->GetUserNameString();
    }
    return "Unknown";
}

std::string FServersSocketData::GetRemoteIP(AnyWebSocket wsVariant) const
{
    std::string IP;
    std::visit([&](auto&& ws) {
        std::string_view RawIP = ws->getRemoteAddressAsText();
        IP = std::string(RawIP);
    }, wsVariant);
    return IP;
}

bool FServersSocketData::IsServerOwner(Uint64 ServerId, Uint64 UserId)
{
    FServersManager* ServersManager = ProjectEngine->GetServersManager();
    const std::shared_ptr<FServer> Server = ServersManager->GetServerById(ServerId);
    if (!Server)
    {
        return false;
    }
    return Server->GetOwnerId() == UserId;
}

nlohmann::json FServersSocketData::BuildRoomDataJson(Uint64 ServerId)
{
    FServersManager* ServersManager = ProjectEngine->GetServersManager();
    auto Server = ServersManager->GetServerById(ServerId);

    nlohmann::json Data;
    if (!Server)
    {
        Data["error"] = "Server not found";
        return Data;
    }

    Data["room_id"] = std::to_string(ServerId);
    Data["room_name"] = Server->GetServerName();
    Data["room_token"] = Server->GetToken();
    Data["owner_id"] = std::to_string(Server->GetOwnerId());
    Data["created_at"] = Server->GetCreatedAt();

    // Build a lookup: user_id -> user_name from members
    std::unordered_map<Uint64, std::string> MemberIdToName;
    for (const auto& Member : Server->GetMembers())
    {
        MemberIdToName[Member.UserId] = Member.UserName;
    }

    // Members (include permissions)
    nlohmann::json MembersArray = nlohmann::json::array();
    for (const auto& Member : Server->GetMembers())
    {
        nlohmann::json MemberJson;
        MemberJson["user_id"] = std::to_string(Member.UserId);
        MemberJson["user_name"] = Member.UserName;
        MemberJson["status"] = Member.Status;
        MemberJson["permissions"] = std::to_string(Member.Permissions);
        MemberJson["is_owner"] = (Member.UserId == Server->GetOwnerId());
        MembersArray.push_back(MemberJson);
    }
    Data["members"] = MembersArray;

    // Channels
    nlohmann::json ChannelsArray = nlohmann::json::array();
    for (const auto& Channel : Server->GetAllChannels())
    {
        nlohmann::json ChannelJson;
        ChannelJson["channel_id"] = std::to_string(Channel->ChannelId);
        ChannelJson["channel_name"] = Channel->ChannelName;
        ChannelJson["channel_type"] = (Channel->ChannelType == EServerChannelType::Voice) ? "voice" : "text";

        // For voice channels: include who is currently connected
        if (Channel->ChannelType == EServerChannelType::Voice && !Channel->ConnectedUsers.empty())
        {
            nlohmann::json ConnectedArray = nlohmann::json::array();
            for (Uint64 ConnectedUserId : Channel->ConnectedUsers)
            {
                nlohmann::json ConnectedJson;
                ConnectedJson["user_id"] = std::to_string(ConnectedUserId);
                // Resolve user name from members map
                auto NameIter = MemberIdToName.find(ConnectedUserId);
                ConnectedJson["user_name"] = (NameIter != MemberIdToName.end()) ? NameIter->second : "Unknown";
                ConnectedArray.push_back(ConnectedJson);
            }
            ChannelJson["connected_users"] = ConnectedArray;
        }

        ChannelsArray.push_back(ChannelJson);
    }
    Data["channels"] = ChannelsArray;

    return Data;
}
