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
        case ESocketMessageServersType::CreateServer:
        {
            if (DataJSON.contains("server_name"))
            {
                CreateServer(wsVariant, opCode, DataJSON["server_name"].get<std::string>());
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing server_name", opCode);
            }
            break;
        }

        case ESocketMessageServersType::JoinServer:
        {
            if (DataJSON.contains("server_id"))
            {
                const Uint64 ServerId = std::stoull(DataJSON["server_id"].get<std::string>());
                JoinServer(wsVariant, opCode, ServerId);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing server_id", opCode);
            }
            break;
        }

        case ESocketMessageServersType::LeaveServer:
        {
            if (DataJSON.contains("server_id"))
            {
                const Uint64 ServerId = std::stoull(DataJSON["server_id"].get<std::string>());
                LeaveServer(wsVariant, opCode, ServerId);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing server_id", opCode);
            }
            break;
        }

        case ESocketMessageServersType::ServerMessage:
        {
            if (DataJSON.contains("server_id") && DataJSON.contains("channel_id") && DataJSON.contains("content"))
            {
                const Uint64 ServerId = std::stoull(DataJSON["server_id"].get<std::string>());
                const Uint64 ChannelId = std::stoull(DataJSON["channel_id"].get<std::string>());
                const std::string Content = DataJSON["content"].get<std::string>();
                ServerMessage(wsVariant, opCode, ServerId, ChannelId, Content);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing server_message fields", opCode);
            }
            break;
        }

        case ESocketMessageServersType::CreateChannel:
        {
            if (DataJSON.contains("server_id") && DataJSON.contains("channel_name") && DataJSON.contains("channel_type"))
            {
                const Uint64 ServerId = std::stoull(DataJSON["server_id"].get<std::string>());
                const std::string ChannelName = DataJSON["channel_name"].get<std::string>();
                const std::string ChannelType = DataJSON["channel_type"].get<std::string>();
                CreateChannel(wsVariant, opCode, ServerId, ChannelName, ChannelType);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing create_channel fields", opCode);
            }
            break;
        }

        case ESocketMessageServersType::MoveChannel:
        {
            if (DataJSON.contains("server_id") && DataJSON.contains("channel_id") && DataJSON.contains("new_position"))
            {
                const Uint64 ServerId = std::stoull(DataJSON["server_id"].get<std::string>());
                const Uint64 ChannelId = std::stoull(DataJSON["channel_id"].get<std::string>());
                const int32 NewPosition = DataJSON["new_position"].get<int32>();
                HandleMoveChannel(wsVariant, opCode, ServerId, ChannelId, NewPosition);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing move_channel fields (server_id, channel_id, new_position)", opCode);
            }
            break;
        }

        case ESocketMessageServersType::ReorderChannels:
        {
            if (DataJSON.contains("server_id") && DataJSON.contains("channel_ids") && DataJSON["channel_ids"].is_array())
            {
                const Uint64 ServerId = std::stoull(DataJSON["server_id"].get<std::string>());
                std::vector<Uint64> ChannelIds;
                for (const nlohmann::basic_json<>& IdJson : DataJSON["channel_ids"])
                {
                    if (IdJson.is_string())
                    {
                        ChannelIds.push_back(std::stoull(IdJson.get<std::string>()));
                    }
                    else if (IdJson.is_number())
                    {
                        ChannelIds.push_back(IdJson.get<Uint64>());
                    }
                }
                HandleReorderChannels(wsVariant, opCode, ServerId, ChannelIds);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing reorder_channels fields (server_id, channel_ids array)", opCode);
            }
            break;
        }

        case ESocketMessageServersType::DeleteChannel:
        {
            if (DataJSON.contains("server_id") && DataJSON.contains("channel_id"))
            {
                const Uint64 ServerId = std::stoull(DataJSON["server_id"].get<std::string>());
                const Uint64 ChannelId = std::stoull(DataJSON["channel_id"].get<std::string>());
                HandleDeleteChannel(wsVariant, opCode, ServerId, ChannelId);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing delete_channel fields (server_id, channel_id)", opCode);
            }
            break;
        }

        case ESocketMessageServersType::RenameChannel:
        {
            if (DataJSON.contains("server_id") && DataJSON.contains("channel_id") && DataJSON.contains("new_name"))
            {
                const Uint64 ServerId = std::stoull(DataJSON["server_id"].get<std::string>());
                const Uint64 ChannelId = std::stoull(DataJSON["channel_id"].get<std::string>());
                const std::string NewName = DataJSON["new_name"].get<std::string>();
                HandleRenameChannel(wsVariant, opCode, ServerId, ChannelId, NewName);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing rename_channel fields (server_id, channel_id, new_name)", opCode);
            }
            break;
        }

        case ESocketMessageServersType::ServerInvite:
        {
            if (DataJSON.contains("server_id") && DataJSON.contains("user_id"))
            {
                const Uint64 ServerId = std::stoull(DataJSON["server_id"].get<std::string>());
                const Uint64 TargetUserId = std::stoull(DataJSON["user_id"].get<std::string>());
                ServerInvite(wsVariant, opCode, ServerId, TargetUserId);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing server_invite fields", opCode);
            }
            break;
        }

        case ESocketMessageServersType::ServerJoinVoice:
        {
            if (DataJSON.contains("server_id") && DataJSON.contains("channel_id"))
            {
                const Uint64 ServerId = std::stoull(DataJSON["server_id"].get<std::string>());
                const Uint64 ChannelId = std::stoull(DataJSON["channel_id"].get<std::string>());
                ServerJoinVoice(wsVariant, opCode, ServerId, ChannelId);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing server_join_voice fields", opCode);
            }
            break;
        }

        case ESocketMessageServersType::ServerLeaveVoice:
        {
            if (DataJSON.contains("server_id") && DataJSON.contains("channel_id"))
            {
                const Uint64 ServerId = std::stoull(DataJSON["server_id"].get<std::string>());
                const Uint64 ChannelId = std::stoull(DataJSON["channel_id"].get<std::string>());
                ServerLeaveVoice(wsVariant, opCode, ServerId, ChannelId);
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
                Uint32 Offset = 0;
                Uint32 Limit = 50;
                if (DataJSON.contains("offset"))
                {
                    if (DataJSON["offset"].is_string())
                        Offset = static_cast<Uint32>(std::stoul(DataJSON["offset"].get<std::string>()));
                    else
                        Offset = DataJSON["offset"].get<Uint32>();
                }
                if (DataJSON.contains("limit"))
                {
                    if (DataJSON["limit"].is_string())
                        Limit = static_cast<Uint32>(std::stoul(DataJSON["limit"].get<std::string>()));
                    else
                        Limit = DataJSON["limit"].get<Uint32>();
                }
                if (Limit == 0 || Limit > 200) Limit = 50;
                HandleGetServerList(wsVariant, opCode, Offset, Limit);
            break;
        }

        case ESocketMessageServersType::GetServerMessages:
        {
            if (DataJSON.contains("server_id") && DataJSON.contains("channel_id"))
            {
                const Uint64 ServerId = std::stoull(DataJSON["server_id"].get<std::string>());
                const Uint64 ChannelId = std::stoull(DataJSON["channel_id"].get<std::string>());
                Uint64 Before = 0;
                Uint32 Limit = 50;

                if (DataJSON.contains("before"))
                    Before = std::stoull(DataJSON["before"].get<std::string>());
                if (DataJSON.contains("limit"))
                    Limit = static_cast<Uint32>(std::stoul(DataJSON["limit"].get<std::string>()));

                if (Limit == 0 || Limit > 100) Limit = 50;

                HandleGetServerMessages(wsVariant, opCode, ServerId, ChannelId, Before, Limit);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing server_id or channel_id", opCode);
            }
            break;
        }

        case ESocketMessageServersType::ServerCreateInvite:
        {
            if (DataJSON.contains("server_id"))
            {
                const Uint64 ServerId = std::stoull(DataJSON["server_id"].get<std::string>());

                // Optional: max uses and expiration time (configured by API caller)
                Uint32 MaxUses = 0;
                Uint32 ExpiresInSeconds = 0;

                if (DataJSON.contains("max_uses"))
                    MaxUses = DataJSON["max_uses"].get<Uint32>();
                if (DataJSON.contains("expires_in_seconds"))
                    ExpiresInSeconds = DataJSON["expires_in_seconds"].get<Uint32>();

                HandleServerCreateInvite(wsVariant, opCode, ServerId, MaxUses, ExpiresInSeconds);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing server_id", opCode);
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
            if (DataJSON.contains("server_id") && DataJSON.contains("target_user_id") && DataJSON.contains("permissions"))
            {
                const Uint64 ServerId = std::stoull(DataJSON["server_id"].get<std::string>());
                const Uint64 TargetUserId = std::stoull(DataJSON["target_user_id"].get<std::string>());
                const Uint64 NewPermissions = std::stoull(DataJSON["permissions"].get<std::string>());

                HandleUpdateMemberPermissions(wsVariant, opCode, ServerId, TargetUserId, NewPermissions);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing field in server_update_member_permissions. Required: server_id, target_user_id, permissions.", opCode);
            }
            break;
        }

        // ========== NEW: Invite management ==========

        case ESocketMessageServersType::ServerDeleteInvite:
        {
            if (DataJSON.contains("server_id") && DataJSON.contains("invite_code"))
            {
                const Uint64 ServerId = std::stoull(DataJSON["server_id"].get<std::string>());
                const std::string InviteCode = DataJSON["invite_code"].get<std::string>();
                HandleServerDeleteInvite(wsVariant, opCode, ServerId, InviteCode);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing server_id or invite_code", opCode);
            }
            break;
        }

        case ESocketMessageServersType::ServerListInvites:
        {
            if (DataJSON.contains("server_id"))
            {
                const Uint64 ServerId = std::stoull(DataJSON["server_id"].get<std::string>());
                Uint32 Start = 0;
                Uint32 Count = 50;

                if (DataJSON.contains("start"))
                    Start = DataJSON["start"].get<Uint32>();
                if (DataJSON.contains("count"))
                    Count = DataJSON["count"].get<Uint32>();

                // Clamp pagination params at socket layer so response values are accurate.
                // Consistent with FServersManager::ListInvites: clamp Count to 200 max, default 50.
                if (Count == 0) Count = 50;
                else if (Count > 200) Count = 200;

                HandleServerListInvites(wsVariant, opCode, ServerId, Start, Count);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing server_id", opCode);
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

void FServersSocketData::CreateServer(AnyWebSocket wsVariant, uWS::OpCode opCode, const std::string& ServerName)
{
    if (ServerName.empty())
    {
        FSocket::EarlyExit(wsVariant, "empty server_name", opCode);
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
    const Uint64 NewServerId = ServersManager->AddServer(ServerName, CurrentUserId);

    if (NewServerId == 0)
    {
        FSocket::EarlyExit(wsVariant, "failed to create server", opCode);
        return;
    }

    // Get the created server to build the response
    auto Server = ServersManager->GetServerById(NewServerId);
    if (!Server)
    {
        FSocket::EarlyExit(wsVariant, "server created but not found", opCode);
        return;
    }

    // Build response with server data
    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerCreated);
    ResponseJson["data"] = BuildServerDataJson(NewServerId);

    // Send confirmation to the creator
    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    LOG_INFO("Server created: '" << ServerName << "' (ID: " << NewServerId << ") by user " << CurrentUserId);
}

void FServersSocketData::JoinServer(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ServerId)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    // Security: JoinServer is only for re-joining servers the user is already a member of.
    // New members must use ServerJoinInvite with a valid invite code.
    if (!ServersManager->IsUserInServer(ServerId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this server. Use server_join_invite with a valid invite code.", opCode);
        return;
    }

    const std::string UserName = GetUserName(CurrentUserId);

    // Send full server data to the re-joining user
    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerCreated);
    ResponseJson["data"] = BuildServerDataJson(ServerId);

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    LOG_INFO("User " << CurrentUserId << " re-joined server " << ServerId);
}

void FServersSocketData::LeaveServer(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ServerId)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    if (!ServersManager->RemoveUserFromServer(ServerId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "failed to leave server", opCode);
        return;
    }

    // Broadcast to remaining members
    nlohmann::json BroadcastJson;
    BroadcastJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerUserLeft);
    BroadcastJson["data"]["server_id"] = ServerId;
    BroadcastJson["data"]["user_id"] = CurrentUserId;

    BroadcastToServerMembers(ServerId, BroadcastJson);

    // Confirm to the leaving user
    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::LeaveServer);
    ResponseJson["data"]["server_id"] = ServerId;
    ResponseJson["data"]["status"] = "left";

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    LOG_INFO("User " << CurrentUserId << " left server " << ServerId);
}

void FServersSocketData::ServerMessage(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ServerId, Uint64 ChannelId, const std::string& Content)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    // Verify user is a member
    if (!ServersManager->IsUserInServer(ServerId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this server", opCode);
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
    const Uint64 MessageId = ServersManager->AddMessage(ServerId, ChannelId, CurrentUserId, UserName, Content);
    if (MessageId == 0)
    {
        FSocket::EarlyExit(wsVariant, "failed to save message", opCode);
        return;
    }

    // Broadcast to all server members (including sender for consistency with frontend)
    nlohmann::json BroadcastJson;
    BroadcastJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerMessage);
    BroadcastJson["data"]["server_id"] = ServerId;
    BroadcastJson["data"]["channel_id"] = ChannelId;
    BroadcastJson["data"]["message_id"] = MessageId;
    BroadcastJson["data"]["sender_id"] = CurrentUserId;
    BroadcastJson["data"]["sender_name"] = UserName;
    BroadcastJson["data"]["content"] = Content;
    BroadcastJson["data"]["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();

    BroadcastToServerMembers(ServerId, BroadcastJson);
}

void FServersSocketData::CreateChannel(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ServerId, const std::string& ChannelName, const std::string& ChannelType)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    if (!ServersManager->IsUserInServer(ServerId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this server", opCode);
        return;
    }

    const EServerChannelType Type = (ChannelType == "voice") ? EServerChannelType::Voice : EServerChannelType::Text;
    const Uint64 NewChannelId = ServersManager->AddChannel(ServerId, ChannelName, Type, CurrentUserId);

    if (NewChannelId == 0)
    {
        FSocket::EarlyExit(wsVariant, "failed to create channel", opCode);
        return;
    }

    // Broadcast to all server members
    nlohmann::json BroadcastJson;
    BroadcastJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerChannelCreated);
    BroadcastJson["data"]["server_id"] = ServerId;
    BroadcastJson["data"]["channel_id"] = NewChannelId;
    BroadcastJson["data"]["channel_name"] = ChannelName;
    BroadcastJson["data"]["channel_type"] = ChannelType;

    BroadcastToServerMembers(ServerId, BroadcastJson);

    LOG_INFO("Channel '" << ChannelName << "' created in server " << ServerId);
}

void FServersSocketData::ServerInvite(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ServerId, Uint64 TargetUserId)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    if (!ServersManager->IsUserInServer(ServerId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this server", opCode);
        return;
    }

    auto Server = ServersManager->GetServerById(ServerId);
    if (!Server)
    {
        FSocket::EarlyExit(wsVariant, "server not found", opCode);
        return;
    }

    const std::string UserName = GetUserName(CurrentUserId);

    // Send invite notification to the target user
    nlohmann::json InviteJson;
    InviteJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerInvite);
    InviteJson["data"]["server_id"] = ServerId;
    InviteJson["data"]["server_name"] = Server->GetServerName();
    InviteJson["data"]["inviter_id"] = CurrentUserId;
    InviteJson["data"]["inviter_name"] = UserName;

    SendToUser(TargetUserId, InviteJson);

    // Confirm to the inviter
    nlohmann::json ConfirmJson;
    ConfirmJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerInvite);
    ConfirmJson["data"]["status"] = "sent";
    ConfirmJson["data"]["server_id"] = ServerId;
    ConfirmJson["data"]["user_id"] = TargetUserId;

    std::visit([&](auto* ws)
    {
        ws->send(ConfirmJson.dump(), opCode);
    }, wsVariant);

    LOG_INFO("User " << CurrentUserId << " invited user " << TargetUserId << " to server " << ServerId);
}

void FServersSocketData::ServerJoinVoice(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ServerId, Uint64 ChannelId)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    if (!ServersManager->IsUserInServer(ServerId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this server", opCode);
        return;
    }

    // Verify channel exists and is voice type
    std::shared_ptr<FServer> Server = ServersManager->GetServerById(ServerId);
    if (!Server)
    {
        FSocket::EarlyExit(wsVariant, "server not found", opCode);
        return;
    }

    std::shared_ptr<FServerChannel> Channel = Server->GetChannel(ChannelId);
    if (!Channel || Channel->ChannelType != EServerChannelType::Voice)
    {
        FSocket::EarlyExit(wsVariant, "invalid voice channel", opCode);
        return;
    }

    ServersManager->JoinVoiceChannel(ServerId, ChannelId, CurrentUserId);

    // Also create/check the Go voice service room
    const std::string VoiceRoomName = "Server_" + std::to_string(ServerId) + "_" + std::to_string(ChannelId);

    const ERoomExistenceStatus CheckResult = ProjectEngine->GetRoomsManager()->CheckRoom(VoiceRoomName);
    if (CheckResult == ERoomExistenceStatus::NotExists)
    {
        ProjectEngine->GetRoomsManager()->CreateRoom(VoiceRoomName);
    }

    const std::string RoomToken = ProjectEngine->GetRoomsManager()->GetRoomToken(VoiceRoomName);
    const std::string UserName = GetUserName(CurrentUserId);

    // Send data_stream_channel info back so frontend can connect to Go service
    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerJoinVoice);
    ResponseJson["data"]["name"] = VoiceRoomName;
    ResponseJson["data"]["token"] = RoomToken;
    ResponseJson["data"]["user_name"] = UserName;

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    // Broadcast voice join to server members
    nlohmann::json BroadcastJson;
    BroadcastJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerUserVoiceJoin);
    BroadcastJson["data"]["server_id"] = ServerId;
    BroadcastJson["data"]["channel_id"] = ChannelId;
    BroadcastJson["data"]["user_id"] = CurrentUserId;
    BroadcastJson["data"]["user_name"] = UserName;

    BroadcastToServerMembers(ServerId, BroadcastJson, CurrentUserId);

    LOG_INFO("User " << CurrentUserId << " joined voice channel " << ChannelId << " in server " << ServerId);
}

void FServersSocketData::ServerLeaveVoice(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 ServerId, Uint64 ChannelId)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    ServersManager->LeaveVoiceChannel(ServerId, ChannelId, CurrentUserId);

    const std::string UserName = GetUserName(CurrentUserId);

    // Confirm to the user
    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerLeaveVoice);
    ResponseJson["data"]["status"] = "disconnected";

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    // Broadcast to server members
    nlohmann::json BroadcastJson;
    BroadcastJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerUserVoiceLeave);
    BroadcastJson["data"]["server_id"] = ServerId;
    BroadcastJson["data"]["channel_id"] = ChannelId;
    BroadcastJson["data"]["user_id"] = CurrentUserId;
    BroadcastJson["data"]["user_name"] = UserName;

    BroadcastToServerMembers(ServerId, BroadcastJson, CurrentUserId);

    LOG_INFO("User " << CurrentUserId << " left voice channel " << ChannelId << " in server " << ServerId);
}

// ========== NEW: WebSocket handlers replacing REST endpoints ==========

void FServersSocketData::HandleGetServerList(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint32 Offset, Uint32 Limit)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();
    const std::vector<std::shared_ptr<FServer>> Servers = ServersManager->GetUserServers(CurrentUserId);

    const Uint32 Total = static_cast<Uint32>(Servers.size());

    // Apply offset/limit pagination for efficient frontend loading
    const Uint32 StartIndex = (Offset < Total) ? Offset : Total;
    const Uint32 EndIndex = (StartIndex + Limit < Total) ? (StartIndex + Limit) : Total;
    const bool bHasMore = (EndIndex < Total);

    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerList);

    nlohmann::json ServersArray = nlohmann::json::array();

    for (Uint32 i = StartIndex; i < EndIndex; ++i)
    {
        ServersArray.push_back(BuildServerDataJson(Servers[i]->GetServerId()));
    }

    ResponseJson["data"]["servers"] = ServersArray;
    ResponseJson["data"]["total"] = Total;
    ResponseJson["data"]["has_more"] = bHasMore;

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);
}

void FServersSocketData::HandleGetServerMessages(AnyWebSocket wsVariant, uWS::OpCode opCode, const Uint64 ServerId, const Uint64 ChannelId, const Uint64 Before, const Uint32 Limit)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    // Verify membership
    if (!ServersManager->IsUserInServer(ServerId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this server", opCode);
        return;
    }

    // Use timestamp-based pagination
    const std::vector<FServerMessage>& Messages = ServersManager->GetChannelMessages(ServerId, ChannelId, Before, Limit);

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
    ResponseJson["data"]["has_more"] = (Messages.size() >= Limit);

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);
}

void FServersSocketData::HandleServerCreateInvite(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                                    const Uint64 ServerId, const Uint32 MaxUses, const Uint32 ExpiresInSeconds)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    // Rate-limit: check hourly invite creation cap per IP
    const std::string ClientIP = GetRemoteIP(wsVariant);
    FAbuseProtection* AbuseProtection = ProjectEngine->GetAbuseProtection();
    if (!ClientIP.empty() && !AbuseProtection->CanAddressCreateInvite(ClientIP))
    {
        FSocket::EarlyExit(wsVariant, "invite create rate limit exceeded", opCode);
        return;
    }
    AbuseProtection->AddCreateInviteAttempt(ClientIP);

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    if (!ServersManager->IsUserInServer(ServerId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this server", opCode);
        return;
    }

    // Check permission: user must have CAN_CREATE_INVITES or be owner
    if (!ServersManager->UserHasPermission(ServerId, CurrentUserId, EServerPermission::CAN_CREATE_INVITES))
    {
        FSocket::EarlyExit(wsVariant, "permission denied: you lack CAN_CREATE_INVITES permission", opCode);
        return;
    }

    const std::string InviteCode = ServersManager->CreateInvite(ServerId, CurrentUserId, MaxUses, ExpiresInSeconds);
    if (InviteCode.empty())
    {
        FSocket::EarlyExit(wsVariant, "failed to create invite", opCode);
        return;
    }

    // Resolve actual defaults used (for response)
    const FBackendSettings* Settings = ProjectEngine->GetBackendSettings();
    const Uint32 ActualMaxUses = (MaxUses > 0) ? MaxUses : static_cast<Uint32>(Settings->GetInviteDefaultMaxUses());
    Uint32 ActualExpiresInSeconds = (ExpiresInSeconds > 0) ? ExpiresInSeconds : static_cast<Uint32>(Settings->GetInviteDefaultExpiresInSeconds());
    if (ActualExpiresInSeconds > static_cast<Uint32>(Settings->GetInviteMaxExpiresInSeconds()))
        ActualExpiresInSeconds = static_cast<Uint32>(Settings->GetInviteMaxExpiresInSeconds());

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

    LOG_INFO("Invite code created for server " << ServerId << " by user " << CurrentUserId
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

    // Rate-limit: check hourly invite use cap per IP
    {
        FAbuseProtection* AbuseProtection = ProjectEngine->GetAbuseProtection();
        if (!ClientIP.empty() && !AbuseProtection->CanAddressUseInvite(ClientIP))
        {
            FSocket::EarlyExit(wsVariant, "invite use rate limit exceeded", opCode);
            return;
        }
        AbuseProtection->AddUseInviteAttempt(ClientIP);
    }

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

    // Send full server data to the joining user
    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerJoined);
    ResponseJson["data"] = BuildServerDataJson(Server->GetServerId());

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    // Broadcast to server members that a new user joined
    nlohmann::json BroadcastJson;
    BroadcastJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerUserJoined);
    BroadcastJson["data"]["server_id"] = Server->GetServerId();
    BroadcastJson["data"]["user_id"] = CurrentUserId;
    BroadcastJson["data"]["user_name"] = UserName;

    BroadcastToServerMembers(Server->GetServerId(), BroadcastJson, CurrentUserId);

    LOG_INFO("User " << CurrentUserId << " joined via invite code to server " << Server->GetServerId());
}

// ========== NEW: Permission Management ==========

void FServersSocketData::HandleUpdateMemberPermissions(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                                         Uint64 ServerId, Uint64 TargetUserId, Uint64 NewPermissions)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    // Only the server owner can manage permissions (future: CAN_MANAGE_PERMISSIONS permission)
    if (!IsServerOwner(ServerId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "permission denied: only the server owner can manage member permissions", opCode);
        return;
    }

    // Cannot modify owner's permissions
    const std::shared_ptr<FServer> Server = ServersManager->GetServerById(ServerId);
    if (Server && Server->GetOwnerId() == TargetUserId)
    {
        FSocket::EarlyExit(wsVariant, "cannot modify the server owners permissions", opCode);
        return;
    }

    if (!ServersManager->IsUserInServer(ServerId, TargetUserId))
    {
        FSocket::EarlyExit(wsVariant, "target user is not a member of this server", opCode);
        return;
    }

    if (!ServersManager->UpdateMemberPermissions(ServerId, TargetUserId, NewPermissions))
    {
        FSocket::EarlyExit(wsVariant, "failed to update member permissions", opCode);
        return;
    }

    // Send confirmation to the requester
    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerMemberPermissionsUpdated);
    ResponseJson["data"]["server_id"] = ServerId;
    ResponseJson["data"]["user_id"] = TargetUserId;
    ResponseJson["data"]["permissions"] = NewPermissions;

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    // Also broadcast the updated server data to all members (so permissions reflect in member list)
    nlohmann::json BroadcastJson;
    BroadcastJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerMemberPermissionsUpdated);
    BroadcastJson["data"]["server_id"] = ServerId;
    BroadcastJson["data"]["user_id"] = TargetUserId;
    BroadcastJson["data"]["permissions"] = NewPermissions;

    BroadcastToServerMembers(ServerId, BroadcastJson);

    LOG_INFO("Permissions updated for user " << TargetUserId << " in server " << ServerId
             << " by owner " << CurrentUserId << " to " << NewPermissions);
}

// ========== NEW: Invite Management ==========

void FServersSocketData::HandleServerDeleteInvite(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                                    Uint64 ServerId, const std::string& InviteCode)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    if (!ServersManager->IsUserInServer(ServerId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this server", opCode);
        return;
    }

    // Check permission: user must have CAN_CREATE_INVITES or be owner
    if (!ServersManager->UserHasPermission(ServerId, CurrentUserId, EServerPermission::CAN_CREATE_INVITES))
    {
        FSocket::EarlyExit(wsVariant, "permission denied: you lack CAN_CREATE_INVITES permission", opCode);
        return;
    }

    if (!ServersManager->DeleteInvite(ServerId, InviteCode, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "invite not found or already deleted", opCode);
        return;
    }

    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerInviteDeleted);
    ResponseJson["data"]["server_id"] = ServerId;
    ResponseJson["data"]["invite_code"] = InviteCode;

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    LOG_INFO("Invite " << InviteCode << " deleted from server " << ServerId
             << " by user " << CurrentUserId);
}

void FServersSocketData::HandleServerListInvites(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                                   Uint64 ServerId, Uint32 Start, Uint32 Count)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    if (!ServersManager->IsUserInServer(ServerId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this server", opCode);
        return;
    }

    // Check permission: user must have CAN_CREATE_INVITES or be owner
    if (!ServersManager->UserHasPermission(ServerId, CurrentUserId, EServerPermission::CAN_CREATE_INVITES))
    {
        FSocket::EarlyExit(wsVariant, "permission denied: you lack CAN_CREATE_INVITES permission", opCode);
        return;
    }

    Uint32 Total = 0;
    auto Invites = ServersManager->ListInvites(ServerId, CurrentUserId, Start, Count, &Total);

    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerInvitesList);
    ResponseJson["data"]["server_id"] = ServerId;
    ResponseJson["data"]["start"] = Start;
    ResponseJson["data"]["count"] = static_cast<Uint32>(Invites.size());
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

    LOG_INFO("Listed " << Invites.size() << " invites for server " << ServerId
             << " (total " << Total << ", page start=" << Start << ") by user " << CurrentUserId);
}

// ========== Channel Management (Move, Delete & Rename) ==========

void FServersSocketData::HandleMoveChannel(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                           const Uint64 ServerId, const Uint64 ChannelId, const int32 NewPosition)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    if (!ServersManager->IsUserInServer(ServerId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this server", opCode);
        return;
    }

    if (!ServersManager->UserHasPermission(ServerId, CurrentUserId, EServerPermission::CAN_MANAGE_CHANNELS))
    {
        FSocket::EarlyExit(wsVariant, "permission denied: you lack CAN_MANAGE_CHANNELS permission", opCode);
        return;
    }

    if (!ServersManager->MoveChannel(ServerId, ChannelId, NewPosition, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "failed to move channel", opCode);
        return;
    }

    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerChannelMoved);
    ResponseJson["data"]["server_id"] = ServerId;
    ResponseJson["data"]["channel_id"] = ChannelId;
    ResponseJson["data"]["new_position"] = NewPosition;

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    BroadcastToServerMembers(ServerId, ResponseJson);

    LOG_INFO("Channel " << ChannelId << " moved to position " << NewPosition
             << " in server " << ServerId << " by user " << CurrentUserId);
}


void FServersSocketData::HandleReorderChannels(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                                const Uint64 ServerId, const std::vector<Uint64>& ChannelIds)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    if (!ServersManager->IsUserInServer(ServerId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this server", opCode);
        return;
    }

    if (!ServersManager->UserHasPermission(ServerId, CurrentUserId, EServerPermission::CAN_MANAGE_CHANNELS))
    {
        FSocket::EarlyExit(wsVariant, "permission denied: you lack CAN_MANAGE_CHANNELS permission", opCode);
        return;
    }

    if (ChannelIds.empty())
    {
        FSocket::EarlyExit(wsVariant, "channel_ids array must not be empty", opCode);
        return;
    }

    if (!ServersManager->ReorderChannels(ServerId, ChannelIds, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "failed to reorder channels", opCode);
        return;
    }

    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerChannelsReordered);
    ResponseJson["data"]["server_id"] = ServerId;

    nlohmann::json IdsArray = nlohmann::json::array();
    for (const Uint64 Id : ChannelIds)
    {
        IdsArray.push_back(Id);
    }
    ResponseJson["data"]["channel_ids"] = IdsArray;

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    BroadcastToServerMembers(ServerId, ResponseJson);

    LOG_INFO("Channels reordered in server " << ServerId << " by user " << CurrentUserId
             << " (" << ChannelIds.size() << " channels)");
}

void FServersSocketData::HandleDeleteChannel(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                                const Uint64 ServerId, const Uint64 ChannelId)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    if (!ServersManager->IsUserInServer(ServerId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this server", opCode);
        return;
    }

    if (!ServersManager->UserHasPermission(ServerId, CurrentUserId, EServerPermission::CAN_MANAGE_CHANNELS))
    {
        FSocket::EarlyExit(wsVariant, "permission denied: you lack CAN_MANAGE_CHANNELS permission", opCode);
        return;
    }

    if (!ServersManager->RemoveChannel(ServerId, ChannelId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "failed to delete channel or channel not found", opCode);
        return;
    }

    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerChannelDeleted);
    ResponseJson["data"]["server_id"] = ServerId;
    ResponseJson["data"]["channel_id"] = ChannelId;

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    BroadcastToServerMembers(ServerId, ResponseJson);

    LOG_INFO("Channel " << ChannelId << " deleted from server " << ServerId
             << " by user " << CurrentUserId);
}

void FServersSocketData::HandleRenameChannel(AnyWebSocket wsVariant, uWS::OpCode opCode,
                                              const Uint64 ServerId, const Uint64 ChannelId, const std::string& NewName)
{
    const Uint64 CurrentUserId = GetUserIdFromWS(wsVariant);
    if (CurrentUserId == 0)
    {
        FSocket::EarlyExit(wsVariant, "not authenticated", opCode);
        return;
    }

    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    if (!ServersManager->IsUserInServer(ServerId, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "not a member of this server", opCode);
        return;
    }

    if (!ServersManager->UserHasPermission(ServerId, CurrentUserId, EServerPermission::CAN_MANAGE_CHANNELS))
    {
        FSocket::EarlyExit(wsVariant, "permission denied: you lack CAN_MANAGE_CHANNELS permission", opCode);
        return;
    }

    if (!ServersManager->RenameChannel(ServerId, ChannelId, NewName, CurrentUserId))
    {
        FSocket::EarlyExit(wsVariant, "failed to rename channel or channel not found", opCode);
        return;
    }

    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerChannelRenamed);
    ResponseJson["data"]["server_id"] = ServerId;
    ResponseJson["data"]["channel_id"] = ChannelId;
    ResponseJson["data"]["new_name"] = NewName;

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    BroadcastToServerMembers(ServerId, ResponseJson);

    LOG_INFO("Channel " << ChannelId << " renamed to '" << NewName << "' in server " << ServerId
             << " by user " << CurrentUserId);
}

// ========== Public: Member Status Broadcast ==========

void FServersSocketData::BroadcastMemberStatus(const Uint64 UserId, const std::string& UserName, const EUserStatus NewStatus)
{
    FServersManager* ServersManager = ProjectEngine->GetServersManager();

    // Get all servers this user is a member of (lightweight: just IDs from DB)
    const auto ServerIds = ServersManager->GetUserServerIds(UserId);

    const std::string StatusStr = UserStatusToString(NewStatus);

    for (const Uint64 ServerId : ServerIds)
    {
        // Update the in-memory member status if server is cached
        std::shared_ptr<FServer> Server = ServersManager->GetServerById(ServerId);
        if (Server)
        {
            Server->UpdateMemberStatus(UserId, StatusStr);
        }

        // Build broadcast message
        nlohmann::json BroadcastJson;
        BroadcastJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerMemberStatus);
        BroadcastJson["data"]["server_id"] = ServerId;
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
    const std::vector<std::pair<Uint64, Uint64>> VoiceChannels = ServersManager->GetUserVoiceChannels(UserId);

    if (VoiceChannels.empty())
    {
        return;
    }

    for (const auto& [ServerId, ChannelId] : VoiceChannels)
    {
        // Remove user from the voice channel in-memory state
        ServersManager->LeaveVoiceChannel(ServerId, ChannelId, UserId);

        // Broadcast to other server members that user left voice
        nlohmann::json BroadcastJson;
        BroadcastJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerUserVoiceLeave);
        BroadcastJson["data"]["server_id"] = ServerId;
        BroadcastJson["data"]["channel_id"] = ChannelId;
        BroadcastJson["data"]["user_id"] = UserId;
        BroadcastJson["data"]["user_name"] = UserName;

        BroadcastToServerMembers(ServerId, BroadcastJson, UserId);

        LOG_INFO("Auto-cleaned voice: User " << UserId << " removed from channel "
                 << ChannelId << " in server " << ServerId);
    }
}

// ========== Private Helpers ==========

void FServersSocketData::BroadcastToServerMembers(const Uint64 ServerId, const nlohmann::json& JsonMessage, const Uint64 ExcludeUserId)
{
    FServersManager* ServersManager = ProjectEngine->GetServersManager();
    const std::shared_ptr<FServer> Server = ServersManager->GetServerById(ServerId);
    if (!Server)
    {
        return;
    }

    const std::vector<FServerMember> Members = Server->GetMembers();
    for (const FServerMember& Member : Members)
    {
        if (Member.UserId == ExcludeUserId)
        {
            continue;
        }

        SendToUser(Member.UserId, JsonMessage);
    }
}

void FServersSocketData::SendToUser(const Uint64 UserId, const nlohmann::json& JsonMessage)
{
    FSocketManager* SocketMgr = ProjectEngine->GetSocketManager();
    FUserManager* UserMgr = ProjectEngine->GetUserManager();

    const std::shared_ptr<FUser> User = UserMgr->GetUserById(UserId);
    if (!User || User->GetUserStatus() != EUserStatus::Online)
    {
        return;
    }

    // Get user WebSocket and enqueue the message
    const int32 SocketId = User->GetSocketId();
    const std::string Payload = JsonMessage.dump();

    FFunctorLambda<void, void*> SocketAccessFunctor = [Payload, UserId](void* ws)
    {
        auto* WebSocket = static_cast<uWS::WebSocket<false, true, FWebSocketSessionData>*>(ws);
        const std::string UserTopic = FSocket::GenerateUserTopic(UserId);
        if (WebSocket->isSubscribed(UserTopic))
        {
            WebSocket->send(Payload, uWS::OpCode::TEXT);
        }
    };

    SocketMgr->EnqueueTaskForUserAtSocket(SocketId, UserId, SocketAccessFunctor);
}

std::string FServersSocketData::GetUserName(const Uint64 UserId)
{
    FUserManager* UserMgr = ProjectEngine->GetUserManager();
    const std::shared_ptr<FUser> User = UserMgr->GetUserById(UserId);
    if (User)
    {
        return User->GetUserNameString();
    }
    return "Unknown";
}

nlohmann::json FServersSocketData::BuildServerDataJson(const Uint64 ServerId)
{
    FServersManager* ServersManager = ProjectEngine->GetServersManager();
    const std::shared_ptr<FServer> Server = ServersManager->GetServerById(ServerId);

    nlohmann::json Json;
    if (!Server)
    {
        return Json;
    }

    Json["server_id"] = std::to_string(Server->GetServerId());
    Json["server_name"] = Server->GetServerName();
    Json["owner_id"] = std::to_string(Server->GetOwnerId());
    Json["token"] = Server->GetToken();
    Json["created_at"] = Server->GetCreatedAt();

    // Channels
    nlohmann::json ChannelsArray = nlohmann::json::array();
    for (const auto& Channel : Server->GetAllChannels())
    {
        nlohmann::json ChannelJson;
        ChannelJson["channel_id"] = std::to_string(Channel->ChannelId);
        ChannelJson["channel_name"] = Channel->ChannelName;
        ChannelJson["channel_type"] = (Channel->ChannelType == EServerChannelType::Text) ? "text" : "voice";
        ChannelJson["position"] = Channel->Position;
        ChannelsArray.push_back(ChannelJson);
    }
    Json["channels"] = ChannelsArray;

    // Members
    nlohmann::json MembersArray = nlohmann::json::array();
    for (const auto& Member : Server->GetMembers())
    {
        nlohmann::json MemberJson;
        MemberJson["user_id"] = std::to_string(Member.UserId);
        MemberJson["user_name"] = Member.UserName;
        MemberJson["status"] = Member.Status;
        MemberJson["permissions"] = Member.Permissions;
        MembersArray.push_back(MemberJson);
    }
    Json["members"] = MembersArray;

    return Json;
}

Uint64 FServersSocketData::GetUserIdFromWS(AnyWebSocket wsVariant)
{
    return std::visit([](auto* ws) -> Uint64
    {
        if (!ws)
        {
            return 0;
        }
        auto* SessionData = static_cast<FWebSocketSessionData*>(ws->getUserData());
        if (!SessionData)
        {
            return 0;
        }
        return SessionData->UserId;
    }, wsVariant);
}

std::string FServersSocketData::GetRemoteIP(AnyWebSocket wsVariant) const
{
    return std::visit([](auto* ws) -> std::string
    {
        if (ws == nullptr)
        {
            return "";
        }

        return std::string(ws->getRemoteAddressAsText());
    }, wsVariant);
}

bool FServersSocketData::IsServerOwner(const Uint64 ServerId, const Uint64 UserId)
{
    FServersManager* ServersManager = ProjectEngine->GetServersManager();
    const std::shared_ptr<FServer> Server = ServersManager->GetServerById(ServerId);
    if (!Server)
    {
        return false;
    }
    return Server->GetOwnerId() == UserId;
}
