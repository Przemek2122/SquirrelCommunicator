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

        case ESocketMessageServersType::RoomInvite:
        {
            if (DataJSON.contains("room_id") && DataJSON.contains("user_id"))
            {
                const Uint64 RoomId = std::stoull(DataJSON["room_id"].get<std::string>());
                const Uint64 TargetUserId = std::stoull(DataJSON["user_id"].get<std::string>());
                RoomInvite(wsVariant, opCode, RoomId, TargetUserId);
            }
            else
            {
                FSocket::EarlyExit(wsVariant, "missing room_invite fields", opCode);
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
                HandleServerCreateInvite(wsVariant, opCode, RoomId);
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
    const std::string UserName = GetUserName(CurrentUserId);

    if (!ServersManager->AddUserToServer(RoomId, CurrentUserId, UserName))
    {
        FSocket::EarlyExit(wsVariant, "failed to join room", opCode);
        return;
    }

    // Broadcast to room members
    nlohmann::json BroadcastJson;
    BroadcastJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::RoomUserJoined);
    BroadcastJson["data"]["room_id"] = RoomId;
    BroadcastJson["data"]["user_id"] = CurrentUserId;
    BroadcastJson["data"]["user_name"] = UserName;

    BroadcastToServerMembers(RoomId, BroadcastJson, CurrentUserId);

    // Send full room data to the joining user
    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::RoomCreated);
    ResponseJson["data"] = BuildRoomDataJson(RoomId);

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    LOG_INFO("User " << CurrentUserId << " joined room " << RoomId);
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
    ResponseJson["status"] = "left";

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

void FServersSocketData::RoomInvite(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 RoomId, Uint64 TargetUserId)
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
    InviteJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::RoomInvite);
    InviteJson["data"]["room_id"] = RoomId;
    InviteJson["data"]["room_name"] = Server->GetServerName();
    InviteJson["data"]["inviter_id"] = CurrentUserId;
    InviteJson["data"]["inviter_name"] = UserName;

    SendToUser(TargetUserId, InviteJson);

    // Confirm to the inviter
    nlohmann::json ConfirmJson;
    ConfirmJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::RoomInvite);
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
    auto Server = ServersManager->GetServerById(RoomId);
    if (!Server)
    {
        FSocket::EarlyExit(wsVariant, "room not found", opCode);
        return;
    }

    auto Channel = Server->GetChannel(ChannelId);
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
    auto Servers = ServersManager->GetUserServers(CurrentUserId);

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

void FServersSocketData::HandleGetServerMessages(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 RoomId, Uint64 ChannelId, Uint64 Before, int32 Limit)
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
    auto Messages = ServersManager->GetChannelMessages(RoomId, ChannelId, Before, Limit);

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

void FServersSocketData::HandleServerCreateInvite(AnyWebSocket wsVariant, uWS::OpCode opCode, Uint64 RoomId)
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

    const std::string InviteCode = ServersManager->CreateInvite(RoomId, CurrentUserId);
    if (InviteCode.empty())
    {
        FSocket::EarlyExit(wsVariant, "failed to create invite", opCode);
        return;
    }

    nlohmann::json ResponseJson;
    ResponseJson["type"] = SocketMessageServersTypeToString(ESocketMessageServersType::ServerInviteCreated);
    ResponseJson["data"]["invite_code"] = InviteCode;
    ResponseJson["data"]["invite_url"] = "https://comm.sqrll.net/invite/" + InviteCode;
    ResponseJson["data"]["expires_at"] = nullptr;

    std::visit([&](auto* ws)
    {
        ws->send(ResponseJson.dump(), opCode);
    }, wsVariant);

    LOG_INFO("Invite code created for room " << RoomId << " by user " << CurrentUserId);
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

    FServersManager* ServersManager = ProjectEngine->GetServersManager();
    auto Server = ServersManager->JoinViaInvite(InviteCode, CurrentUserId, UserName);

    if (!Server)
    {
        FSocket::EarlyExit(wsVariant, "invalid or expired invite code", opCode);
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

std::string FServersSocketData::GetUserName(Uint64 UserId)
{
    auto User = ProjectEngine->GetUserManager()->GetUserById(UserId);
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

    // Build a lookup: user_id → user_name from members
    std::unordered_map<Uint64, std::string> MemberIdToName;
    for (const auto& Member : Server->GetMembers())
    {
        MemberIdToName[Member.UserId] = Member.UserName;
    }

    // Members
    nlohmann::json MembersArray = nlohmann::json::array();
    for (const auto& Member : Server->GetMembers())
    {
        nlohmann::json MemberJson;
        MemberJson["user_id"] = std::to_string(Member.UserId);
        MemberJson["user_name"] = Member.UserName;
        MemberJson["status"] = Member.Status;
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
