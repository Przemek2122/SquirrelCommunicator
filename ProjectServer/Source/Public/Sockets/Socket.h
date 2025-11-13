#pragma once

#include "Misc/WebSockets/AppWrapper.h"

/** Single web socket */
class FSocket
{
public:
	FSocket(int32 InPort, bool bInUseSSL, std::string InKeyPath, std::string InCertPath);
	~FSocket();

	void Async();

protected:
	int32 Port;
	bool bUseSSL;
	std::string KeyPath;
	std::string CertPath;

	FSocketAppWrapper SocketAppWrapper;
	us_listen_socket_t* AppListenSocket;

};
