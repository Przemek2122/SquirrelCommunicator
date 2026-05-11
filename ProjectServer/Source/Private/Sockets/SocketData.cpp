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
		case HashString("priv"):		return ESocketMessageSection::Priv;
		case HashString("rooms"):	return ESocketMessageSection::Rooms;
		case HashString("error"):	return ESocketMessageSection::Error;
		default:						return ESocketMessageSection::Unknown;
	}
}

std::string SocketMessageSectionToString(const ESocketMessageSection InTypeEnum)
{
	switch (InTypeEnum)
	{
		case ESocketMessageSection::Priv:		return "priv";
		case ESocketMessageSection::Rooms:		return "rooms";
		case ESocketMessageSection::Error:		return "error";
		case ESocketMessageSection::Unknown:
		default:								return "unknown";
	}
}

ESocketMessagePrivateType StringToSocketMessagePrivateType(const std::string_view InTypeString)
{
	// The switch operates on integer hashes calculated at compile-time
	switch (HashString(InTypeString))
	{
		case HashString("message"):					return ESocketMessagePrivateType::Message;
		case HashString("typing"):					return ESocketMessagePrivateType::Typing;
		case HashString("mark_read"):				return ESocketMessagePrivateType::MarkRead;
		case HashString("user_status"):				return ESocketMessagePrivateType::UserStatus;
		case HashString("search_user"):				return ESocketMessagePrivateType::SearchUser;
		case HashString("load_more_messages"):		return ESocketMessagePrivateType::LoadMoreMessages;
		case HashString("get_conversations"):		return ESocketMessagePrivateType::GetConversations;
		case HashString("add_conversation"):			return ESocketMessagePrivateType::AddConversation;
		case HashString("get_friend_list"):			return ESocketMessagePrivateType::GetFriendList;
		case HashString("get_friend_request_list"):	return ESocketMessagePrivateType::GetFriendRequestList;
		case HashString("initial_client_data"):		return ESocketMessagePrivateType::InitialClientData;
		case HashString("initial_conversations"):	return ESocketMessagePrivateType::InitialConversations;
		case HashString("create_friend_request"):	return ESocketMessagePrivateType::CreateFriendRequest;
		case HashString("accept_friend_request"):	return ESocketMessagePrivateType::AcceptFriendRequest;
		case HashString("reject_friend_request"):	return ESocketMessagePrivateType::RejectFriendRequest;
		case HashString("cancel_friend_request"):	return ESocketMessagePrivateType::CancelFriendRequest;
		case HashString("remove_friend"):			return ESocketMessagePrivateType::RemoveFriend;

		case HashString("error"):					return ESocketMessagePrivateType::Error;
		default:										return ESocketMessagePrivateType::Unknown;
	}
}

std::string SocketMessagePrivateTypeToString(const ESocketMessagePrivateType InTypeEnum)
{
	switch (InTypeEnum)
	{
		case ESocketMessagePrivateType::Message:				return "message";
		case ESocketMessagePrivateType::Typing:					return "typing";
		case ESocketMessagePrivateType::MarkRead:				return "mark_read";
		case ESocketMessagePrivateType::UserStatus:				return "user_status";
		case ESocketMessagePrivateType::SearchUser:				return "search_user";
		case ESocketMessagePrivateType::LoadMoreMessages:		return "load_more_messages";
		case ESocketMessagePrivateType::GetConversations:		return "get_conversations";
		case ESocketMessagePrivateType::AddConversation:		return "add_conversation";
		case ESocketMessagePrivateType::GetFriendList:			return "get_friend_list";
		case ESocketMessagePrivateType::GetFriendRequestList:	return "get_friend_request_list";
		case ESocketMessagePrivateType::InitialClientData:		return "initial_client_data";
		case ESocketMessagePrivateType::InitialConversations:	return "initial_conversations";
		case ESocketMessagePrivateType::CreateFriendRequest:	return "create_friend_request";
		case ESocketMessagePrivateType::AcceptFriendRequest:	return "accept_friend_request";
		case ESocketMessagePrivateType::RejectFriendRequest:	return "reject_friend_request";
		case ESocketMessagePrivateType::CancelFriendRequest:	return "cancel_friend_request";
		case ESocketMessagePrivateType::RemoveFriend:			return "remove_friend";

		case ESocketMessagePrivateType::Error:					return "error";
		case ESocketMessagePrivateType::Unknown:
		default:												return "unknown";
	}
}

ESocketMessageRoomsType StringToSocketMessageRoomsType(std::string_view InTypeString)
{
	switch (HashString(InTypeString))
	{
		case HashString("create_room"):		return ESocketMessageRoomsType::CreateRoom;
		case HashString("error"):			return ESocketMessageRoomsType::Error;

		default:								return ESocketMessageRoomsType::Unknown;
	}
}

std::string SocketMessageRoomsTypeToString(ESocketMessageRoomsType InTypeEnum)
{
	switch (InTypeEnum)
	{
		case ESocketMessageRoomsType::CreateRoom:		return "create_room";
		case ESocketMessageRoomsType::Error:			return "error";

		case ESocketMessageRoomsType::Unknown:
		default:										return "unknown";
	}
}
