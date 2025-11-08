#include "Sockets/Socket.h"
#include "Sockets/PerSocketData.h"

FSocket::FSocket(const int32 InPort, bool bInUseSSL, const std::string& InKeyPath, const std::string& InCertPath)
	: Port(InPort)
	, bUseSSL(bInUseSSL)
	, KeyPath(InKeyPath)
	, CertPath(InCertPath)
	, AppListenSocket(nullptr)
	, SocketAppWrapper(FSocketAppWrapper(bInUseSSL, InKeyPath, InCertPath))
{
}

FSocket::~FSocket()
{
	if (AppListenSocket != nullptr)
	{
		us_listen_socket_close(bUseSSL, AppListenSocket);
	}
}

void FSocket::InitAsync()
{
	static const char* WebSocketPath = "/api/v1/ws";

	if (SocketAppWrapper.IsUsingSSL())
	{
		SocketAppWrapper.wsssl<FPerSocketData>(WebSocketPath, {
			.compression = uWS::DISABLED, // @TODO Temporary
			.maxPayloadLength = 16 * 1024, // @TODO Random magic copied from network
			.idleTimeout = 120,
			.open = [](auto* ws)
			{
				LOG_INFO("WebSocket connected!");
			},
			.message = [](auto* ws, std::string_view message, uWS::OpCode)
			{
				ws->send(message, uWS::OpCode::TEXT);
			},
			.close = [](auto* ws, int code, std::string_view message)
			{
				LOG_INFO("WebSocket closed!");
			}
		});
	}
	else
	{
		SocketAppWrapper.wssslno<FPerSocketData>(WebSocketPath, {
			.compression = uWS::DISABLED, // @TODO Temporary
			.maxPayloadLength = 16 * 1024, // @TODO Random magic copied from network
			.idleTimeout = 120,
			.open = [](auto* ws)
			{
				LOG_INFO("WebSocket connected!");
			},
			.message = [](auto* ws, std::string_view message, uWS::OpCode)
			{
				ws->send(message, uWS::OpCode::TEXT);
			},
			.close = [](auto* ws, int code, std::string_view message)
			{
				LOG_INFO("WebSocket closed!");
			}
		});
	}

	SocketAppWrapper.listen(Port, [this](us_listen_socket_t* Socket)
	{
		if (Socket != nullptr)
		{
			AppListenSocket = Socket;

			LOG_INFO("Server listening on port " << std::to_string(Port));
		}
		else
		{
			LOG_ERROR("Failed to listen on port " << std::to_string(Port));
		}
	});

	SocketAppWrapper.Run();
}
