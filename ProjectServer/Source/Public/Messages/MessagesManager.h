#pragma once

#include "CoreMinimal.h"

/**
 * Class for managing user messages
 */
class FMessagesManager
{
public:
	void SendMessageToUser(const Uint64 InUserIndex, const std::string& InUserMessage);
	void GetMessageFromUser(const Uint64 InUserIndex, const std::string& InUserMessage);



};
