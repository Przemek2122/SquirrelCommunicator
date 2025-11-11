#include "Sockets/Socket.h"

FSocket::FSocket(const int32 InPort, bool bInUseSSL, const std::string& InKeyPath, const std::string& InCertPath)
	: Port(InPort)
	, bUseSSL(bInUseSSL)
	, KeyPath(InKeyPath)
	, CertPath(InCertPath)
{
}

FSocket::~FSocket()
{

}

void FSocket::InitAsync()
{
	static const char* WebSocketPath = "/api/v1/ws";

	
}
