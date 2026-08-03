// Created by https://www.linkedin.com/in/przemek2122/ 2026

#include "Sockets/SocketData.h"

// Simple FNV-1a hash function for compile-time usage
constexpr uint32_t HashString(const std::string_view str) {
    uint32_t hash = 2166136261u; // Seed
    for (const char c : str) {
        hash ^= static_cast<uint32_t>(c);
        hash *= 16777619u; // The FNV Prime
    }
    return hash;
}

ESocketMessageSection StringToSocketMessageSection(const std::string_view InTypeString)
{
    // The compiler replaces HashString("...") with a literal number.
    // This switch is extremely fast and easy to read.
    switch (HashString(InTypeString))
    {
        case HashString("priv"):    return ESocketMessageSection::Priv;
        case HashString("servers"): return ESocketMessageSection::Servers;
        case HashString("error"):   return ESocketMessageSection::Error;
        default:                    return ESocketMessageSection::Unknown;
    }
}

std::string SocketMessageSectionToString(const ESocketMessageSection InTypeEnum)
{
    switch (InTypeEnum)
    {
        case ESocketMessageSection::Priv:       return "priv";
        case ESocketMessageSection::Servers:    return "servers";
        case ESocketMessageSection::Error:      return "error";
        case ESocketMessageSection::Unknown:
        default:                                return "unknown";
    }
}

ESocketMessagePrivateType StringToSocketMessagePrivateType(const std::string_view InTypeString)
{
    // The switch operates on integer hashes calculated at compile-time
    switch (HashString(InTypeString))
    {
        case HashString("message"):                 return ESocketMessagePrivateType::Message;
        case HashString("message_edit"):            return ESocketMessagePrivateType::MessageEdit;
        case HashString("typing"):                  return ESocketMessagePrivateType::Typing;
        case HashString("message_delivered"):       return ESocketMessagePrivateType::MessageDelivered;
        case HashString("message_read"):            return ESocketMessagePrivateType::MessageRead;
        case HashString("user_status"):             return ESocketMessagePrivateType::UserStatus;
        case HashString("search_user"):             return ESocketMessagePrivateType::SearchUser;
        case HashString("load_more_messages"):      return ESocketMessagePrivateType::LoadMoreMessages;
        case HashString("get_conversations"):       return ESocketMessagePrivateType::GetConversations;
        case HashString("add_conversation"):        return ESocketMessagePrivateType::AddConversation;
        case HashString("get_friend_list"):         return ESocketMessagePrivateType::GetFriendList;
        case HashString("get_friend_request_list"): return ESocketMessagePrivateType::GetFriendRequestList;
        case HashString("initial_client_data"):     return ESocketMessagePrivateType::InitialClientData;
        case HashString("initial_conversations"):   return ESocketMessagePrivateType::InitialConversations;
        case HashString("create_friend_request"):   return ESocketMessagePrivateType::CreateFriendRequest;
        case HashString("accept_friend_request"):   return ESocketMessagePrivateType::AcceptFriendRequest;
        case HashString("reject_friend_request"):   return ESocketMessagePrivateType::RejectFriendRequest;
        case HashString("cancel_friend_request"):   return ESocketMessagePrivateType::CancelFriendRequest;
        case HashString("remove_friend"):           return ESocketMessagePrivateType::RemoveFriend;
        case HashString("data_stream_channel"):     return ESocketMessagePrivateType::DataStreamChannel;
        case HashString("user_calling"):            return ESocketMessagePrivateType::UserCalling;

        case HashString("error"):                   return ESocketMessagePrivateType::Error;
        default:                                    return ESocketMessagePrivateType::Unknown;
    }
}

std::string SocketMessagePrivateTypeToString(const ESocketMessagePrivateType InTypeEnum)
{
    switch (InTypeEnum)
    {
        case ESocketMessagePrivateType::Message:                return "message";
        case ESocketMessagePrivateType::MessageEdit:            return "message_edit";
        case ESocketMessagePrivateType::Typing:                 return "typing";
        case ESocketMessagePrivateType::MessageDelivered:       return "message_delivered";
        case ESocketMessagePrivateType::MessageRead:            return "message_read";
        case ESocketMessagePrivateType::UserStatus:             return "user_status";
        case ESocketMessagePrivateType::SearchUser:             return "search_user";
        case ESocketMessagePrivateType::LoadMoreMessages:       return "load_more_messages";
        case ESocketMessagePrivateType::GetConversations:       return "get_conversations";
        case ESocketMessagePrivateType::AddConversation:        return "add_conversation";
        case ESocketMessagePrivateType::GetFriendList:          return "get_friend_list";
        case ESocketMessagePrivateType::GetFriendRequestList:   return "get_friend_request_list";
        case ESocketMessagePrivateType::InitialClientData:      return "initial_client_data";
        case ESocketMessagePrivateType::InitialConversations:   return "initial_conversations";
        case ESocketMessagePrivateType::CreateFriendRequest:    return "create_friend_request";
        case ESocketMessagePrivateType::AcceptFriendRequest:    return "accept_friend_request";
        case ESocketMessagePrivateType::RejectFriendRequest:    return "reject_friend_request";
        case ESocketMessagePrivateType::CancelFriendRequest:    return "cancel_friend_request";
        case ESocketMessagePrivateType::RemoveFriend:           return "remove_friend";
        case ESocketMessagePrivateType::DataStreamChannel:      return "data_stream_channel";
        case ESocketMessagePrivateType::UserCalling:            return "user_calling";

        case ESocketMessagePrivateType::Error:                  return "error";
        case ESocketMessagePrivateType::Unknown:
        default:                                                return "unknown";
    }
}

ESocketMessageServersType StringToSocketMessageServersType(std::string_view InTypeString)
{
    switch (HashString(InTypeString))
    {
        // Client -> Server
        case HashString("create_room"):             return ESocketMessageServersType::CreateRoom;
        case HashString("join_room"):               return ESocketMessageServersType::JoinRoom;
        case HashString("leave_room"):              return ESocketMessageServersType::LeaveRoom;
        case HashString("server_message"):          return ESocketMessageServersType::RoomMessage;
        case HashString("create_channel"):          return ESocketMessageServersType::CreateChannel;
        case HashString("move_channel"):            return ESocketMessageServersType::MoveChannel;
        case HashString("reorder_channels"):        return ESocketMessageServersType::ReorderChannels;
        case HashString("delete_channel"):          return ESocketMessageServersType::DeleteChannel;
        case HashString("rename_channel"):          return ESocketMessageServersType::RenameChannel;
        case HashString("server_invite"):           return ESocketMessageServersType::ServerInvite;
        case HashString("server_join_voice"):       return ESocketMessageServersType::RoomJoinVoice;
        case HashString("server_leave_voice"):      return ESocketMessageServersType::RoomLeaveVoice;
        case HashString("get_server_list"):         return ESocketMessageServersType::GetServerList;
        case HashString("get_server_messages"):     return ESocketMessageServersType::GetServerMessages;
        case HashString("server_create_invite"):    return ESocketMessageServersType::ServerCreateInvite;
        case HashString("server_join_invite"):      return ESocketMessageServersType::ServerJoinInvite;
        case HashString("server_update_member_permissions"): return ESocketMessageServersType::ServerUpdateMemberPermissions;
        case HashString("server_delete_invite"):    return ESocketMessageServersType::ServerDeleteInvite;
        case HashString("server_list_invites"):     return ESocketMessageServersType::ServerListInvites;

        // Server -> Client
        case HashString("room_created"):            return ESocketMessageServersType::RoomCreated;
        case HashString("room_user_joined"):        return ESocketMessageServersType::RoomUserJoined;
        case HashString("room_user_left"):          return ESocketMessageServersType::RoomUserLeft;
        case HashString("room_channel_created"):    return ESocketMessageServersType::RoomChannelCreated;
        case HashString("room_channel_moved"):      return ESocketMessageServersType::RoomChannelMoved;
        case HashString("room_channels_reordered"): return ESocketMessageServersType::RoomChannelsReordered;
        case HashString("room_channel_deleted"):    return ESocketMessageServersType::RoomChannelDeleted;
        case HashString("room_channel_renamed"):    return ESocketMessageServersType::RoomChannelRenamed;
        case HashString("server_voice_joined"):     return ESocketMessageServersType::RoomUserVoiceJoin;
        case HashString("server_voice_left"):       return ESocketMessageServersType::RoomUserVoiceLeave;
        case HashString("room_member_status"):      return ESocketMessageServersType::RoomMemberStatus;
        case HashString("server_list"):             return ESocketMessageServersType::ServerList;
        case HashString("server_messages"):         return ESocketMessageServersType::ServerMessages;
        case HashString("server_invite_created"):   return ESocketMessageServersType::ServerInviteCreated;
        case HashString("server_joined"):           return ESocketMessageServersType::ServerJoined;
        case HashString("server_member_permissions_updated"): return ESocketMessageServersType::ServerMemberPermissionsUpdated;
        case HashString("server_invite_deleted"):   return ESocketMessageServersType::ServerInviteDeleted;
        case HashString("server_invites_list"):     return ESocketMessageServersType::ServerInvitesList;

        case HashString("error"):                   return ESocketMessageServersType::Error;

        default:                                    return ESocketMessageServersType::Unknown;
    }
}

std::string SocketMessageServersTypeToString(ESocketMessageServersType InTypeEnum)
{
    switch (InTypeEnum)
    {
        // Client -> Server
        case ESocketMessageServersType::CreateRoom:           return "create_room";
        case ESocketMessageServersType::JoinRoom:             return "join_room";
        case ESocketMessageServersType::LeaveRoom:            return "leave_room";
        case ESocketMessageServersType::RoomMessage:          return "server_message";
        case ESocketMessageServersType::CreateChannel:        return "create_channel";
        case ESocketMessageServersType::MoveChannel:          return "move_channel";
        case ESocketMessageServersType::ReorderChannels:      return "reorder_channels";
        case ESocketMessageServersType::DeleteChannel:        return "delete_channel";
        case ESocketMessageServersType::RenameChannel:        return "rename_channel";
        case ESocketMessageServersType::ServerInvite:         return "server_invite";
        case ESocketMessageServersType::RoomJoinVoice:        return "server_join_voice";
        case ESocketMessageServersType::RoomLeaveVoice:       return "server_leave_voice";
        case ESocketMessageServersType::GetServerList:        return "get_server_list";
        case ESocketMessageServersType::GetServerMessages:    return "get_server_messages";
        case ESocketMessageServersType::ServerCreateInvite:   return "server_create_invite";
        case ESocketMessageServersType::ServerJoinInvite:     return "server_join_invite";
        case ESocketMessageServersType::ServerUpdateMemberPermissions: return "server_update_member_permissions";
        case ESocketMessageServersType::ServerDeleteInvite:   return "server_delete_invite";
        case ESocketMessageServersType::ServerListInvites:    return "server_list_invites";

        // Server -> Client
        case ESocketMessageServersType::RoomCreated:          return "room_created";
        case ESocketMessageServersType::RoomUserJoined:       return "room_user_joined";
        case ESocketMessageServersType::RoomUserLeft:         return "room_user_left";
        case ESocketMessageServersType::RoomChannelCreated:   return "room_channel_created";
        case ESocketMessageServersType::RoomChannelMoved:     return "room_channel_moved";
        case ESocketMessageServersType::RoomChannelsReordered: return "room_channels_reordered";
        case ESocketMessageServersType::RoomChannelDeleted:   return "room_channel_deleted";
        case ESocketMessageServersType::RoomChannelRenamed:   return "room_channel_renamed";
        case ESocketMessageServersType::RoomUserVoiceJoin:    return "server_voice_joined";
        case ESocketMessageServersType::RoomUserVoiceLeave:   return "server_voice_left";
        case ESocketMessageServersType::RoomMemberStatus:     return "room_member_status";
        case ESocketMessageServersType::ServerList:           return "server_list";
        case ESocketMessageServersType::ServerMessages:       return "server_messages";
        case ESocketMessageServersType::ServerInviteCreated:  return "server_invite_created";
        case ESocketMessageServersType::ServerJoined:         return "server_joined";
        case ESocketMessageServersType::ServerMemberPermissionsUpdated: return "server_member_permissions_updated";
        case ESocketMessageServersType::ServerInviteDeleted:  return "server_invite_deleted";
        case ESocketMessageServersType::ServerInvitesList:    return "server_invites_list";

        case ESocketMessageServersType::Error:                return "error";

        case ESocketMessageServersType::Unknown:
        default:                                              return "unknown";
    }
}
