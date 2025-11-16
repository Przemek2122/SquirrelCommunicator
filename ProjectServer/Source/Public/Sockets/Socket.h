#pragma once

#include "Misc/WebSockets/AppWrapper.h"

/** Message structure for user messages */
struct Message
{
	std::string Type;      // "chat", "status", "typing", "seen"
	std::string Target;    // User/room ID
	std::string Content;   // Actual message
	int64_t Timestamp;
};

/** Single web socket */
class FSocket
{
public:
	FSocket(int32 InPort, bool bInUseSSL, std::string InKeyPath, std::string InCertPath);
	~FSocket();

	void Async();
	void OnMessageReceived(auto* ws, std::string_view message, uWS::OpCode opCode);

protected:
	int32 Port;
	bool bUseSSL;
	std::string KeyPath;
	std::string CertPath;

	FSocketAppWrapper SocketAppWrapper;
	us_listen_socket_t* AppListenSocket;

};
