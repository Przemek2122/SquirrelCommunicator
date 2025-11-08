#pragma once

#include "Sockets/SocketAppWraper.h"

/** Single web socket */
class FSocket
{
public:
	FSocket(int32 InPort, bool bInUseSSL, const std::string& InKeyPath, const std::string& InCertPath);
	~FSocket();

	void InitAsync();

protected:
	int32 Port;
	bool bUseSSL;
	std::string KeyPath;
	std::string CertPath;

	us_listen_socket_t* AppListenSocket;
	FSocketAppWrapper SocketAppWrapper;

};
