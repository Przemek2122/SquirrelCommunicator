#pragma once
#include <uwebsockets/WebSocket.h>

struct FWebSocketSessionData
{
	/** Session user Id */
	Uint64 UserId;

	/** User IP Address (sensitive) */
	std::string_view ClientIP;
};
