#pragma once
#include "EngineCompat.h"
#include <uwebsockets/WebSocket.h>
#include <string>

struct FWebSocketSessionData
{
	/** Session user Id */
	Uint64 UserId;

	/** User IP Address (sensitive) */
	std::string_view ClientIP;

	/** Auth session token (used to look up the per-session image API key). */
	std::string SessionToken;
};
