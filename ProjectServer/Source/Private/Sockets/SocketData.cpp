// Created by https://www.linkedin.com/in/przemek2122/ 2026

#include "Sockets/SocketData.h"

ESocketMessageType StringToSocketMessageType(const std::string& InTypeString)
{
	if (InTypeString == "message")					return ESocketMessageType::Message;
	if (InTypeString == "typing")					return ESocketMessageType::Typing;
	if (InTypeString == "mark_read")				return ESocketMessageType::MarkRead;
	if (InTypeString == "user_status")				return ESocketMessageType::UserStatus;
	if (InTypeString == "search_user")				return ESocketMessageType::SearchUser;
	if (InTypeString == "request_add_user")			return ESocketMessageType::RequestAddUser;
	if (InTypeString == "load_more_messages")		return ESocketMessageType::LoadMoreMessages;
	if (InTypeString == "get_conversations")		return ESocketMessageType::GetConversations;
	if (InTypeString == "add_conversation")			return ESocketMessageType::AddConversation;
	if (InTypeString == "initial_client_data")		return ESocketMessageType::InitialClientData;
	if (InTypeString == "initial_conversations")	return ESocketMessageType::InitialConversations;
	if (InTypeString == "error")					return ESocketMessageType::Error;

	return ESocketMessageType::Unknown;
}

std::string SocketMessageTypeToString(const ESocketMessageType InTypeEnum)
{
	switch (InTypeEnum)
	{
		case ESocketMessageType::Message:				return "message";
		case ESocketMessageType::Typing:				return "typing";
		case ESocketMessageType::MarkRead:				return "mark_read";
		case ESocketMessageType::UserStatus:			return "user_status";
		case ESocketMessageType::SearchUser:			return "search_user";
		case ESocketMessageType::RequestAddUser:		return "request_add_user";
		case ESocketMessageType::LoadMoreMessages:		return "load_more_messages";
		case ESocketMessageType::GetConversations:		return "get_conversations";
		case ESocketMessageType::AddConversation:		return "add_conversation";
		case ESocketMessageType::InitialClientData:		return "initial_client_data";
		case ESocketMessageType::InitialConversations:	return "initial_conversations";
		case ESocketMessageType::Error:					return "error";

		case ESocketMessageType::Unknown:
		default:										return "unknown";
	}
}