#include "Sockets/Socket.h"

#include "ProjectEngine.h"
#include "AbuseProtection/AbuseProtection.h"
#include "Auth/UserManager.h"
#include "Misc/WebSockets/CookieHelper.h"
#include "Sockets/WebSocketSession.h"

FSocket::FSocket(const int32 InPort, bool bInUseSSL, std::string InKeyPath, std::string InCertPath)
	: Port(InPort)
	, bUseSSL(bInUseSSL)
	, KeyPath(std::move(InKeyPath))
	, CertPath(std::move(InCertPath))
	, SocketAppWrapper(bInUseSSL, KeyPath, CertPath)
	, AppListenSocket(nullptr)
{
}

FSocket::~FSocket()
{
	if (AppListenSocket != nullptr)
	{
		us_listen_socket_close(0, AppListenSocket);
	}
}


template<bool SSL>
auto CreateSocketBehavior() {
	return typename uWS::TemplatedApp<SSL>::template WebSocketBehavior<FWebSocketSession>{
		/* Settings */
		.compression = uWS::SHARED_COMPRESSOR,
		.maxPayloadLength = 16 * 1024,
		.idleTimeout = 10,
		.maxBackpressure = 1 * 1024 * 1024,
		/* Handlers */
		.upgrade = [](/** uWS::HttpResponse<SSL> */ auto* res, uWS::HttpRequest* req, auto* context)
		{
			auto CookieHeader = req->getHeader("cookie");
			const std::string AuthTokenValue = FCookieHelper::GetCookieValue(CookieHeader, "auth_token");

			/** Upgrade data used when connecting new client */
			struct FSocketUpgradeData
			{
				std::string secWebSocketKey;
				std::string secWebSocketProtocol;
				std::string secWebSocketExtensions;
				us_socket_context_t* context;
				decltype(res) httpRes;
				bool aborted = false;
			};

			auto upgradeData = std::make_shared<FSocketUpgradeData>(FSocketUpgradeData{
				.secWebSocketKey = std::string(req->getHeader("sec-websocket-key")),
				.secWebSocketProtocol = std::string(req->getHeader("sec-websocket-protocol")),
				.secWebSocketExtensions = std::string(req->getHeader("sec-websocket-extensions")),
				.context = context,
				.httpRes = res,
				.aborted = false
				});

			/* We have to attach an abort handler for us to be aware of disconnections while we perform task */
			res->onAborted([=]()
				{
					/* We don't implement any kind of cancellation here, so simply flag us as aborted */
					upgradeData->aborted = true;

					LOG_DEBUG("HTTP socket was closed before we upgraded it!");
				});

			bool bIsTokenValid = false;
			if (!AuthTokenValue.empty())
			{
				FProjectEngine* ProjectEngine = dynamic_cast<FProjectEngine*>(FGlobalDefines::GEngine);
				FUserManager* UserManager = ProjectEngine->GetUserManager();
				if (UserManager->VerifyToken(AuthTokenValue))
				{
					bIsTokenValid = true;
				}
				else
				{
					std::string_view clientIp = res->getRemoteAddressAsText();

					// If behind proxy (nginx, cloudflare), check forwarded header
					std::string_view forwarded = req->getHeader("x-forwarded-for");
					if (!forwarded.empty())
					{
						clientIp = forwarded; // Use forwarded IP instead
					}

					ProjectEngine->GetAbuseProtection()->AddRateLimitedAttempt(std::string(clientIp));
				}
			}

			if (bIsTokenValid)
			{
				// Accept
				res->template upgrade<FWebSocketSession>(
					{},  // userData
					req->getHeader("sec-websocket-key"),
					req->getHeader("sec-websocket-protocol"),
					req->getHeader("sec-websocket-extensions"),
					context
				);
			}
			else
			{
				res->writeStatus("401 Unauthorized");
				res->end("Invalid auth token");
				return;
			}
		},
		.open = [](auto* ws)
		{
			std::cout << "Client connected\n";
		},
		.message = [](auto* ws, std::string_view message, uWS::OpCode opCode)
		{
			std::cout << "Received: " << message << "\n";

			ws->send(message, opCode); // Echo back
		},
		.close = [](auto* ws, int code, std::string_view message)
		{
			std::cout << "Client disconnected\n";
		}
	};
}

void FSocket::Async()
{
	static const char* WebSocketPath = "/api/v1/ws";

	if (bUseSSL)
	{
		SocketAppWrapper.wsssl<FWebSocketSession>(WebSocketPath, CreateSocketBehavior<true>());
	}
	else
	{
		SocketAppWrapper.wssslno<FWebSocketSession>(WebSocketPath, CreateSocketBehavior<false>());
	}

	SocketAppWrapper.listen(Port, [&](us_listen_socket_t* listenSocket)
	{
		if (listenSocket != nullptr)
		{
			AppListenSocket = listenSocket;

			LOG_INFO("Server listening on port: " << Port);
		}
		else
		{
			LOG_ERROR("Failed to listen on port: " << Port);
		}
	});

	SocketAppWrapper.Run();
}
