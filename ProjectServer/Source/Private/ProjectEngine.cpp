#include "ProjectEngine.h"

#include "PredefinedMessages.h"
#include "AbuseProtection/AbuseProtection.h"
#include "Auth/UserManager.h"
#include "Assets/IniReader/IniObject.h"
#include "DataBase/DataBaseSettings.h"
#include "Managers/ConversationsManager.h"
#include "Misc/WebSockets/CookieHelper.h"
#include "Sockets/SocketManager.h"

FProjectEngine::FProjectEngine()
	: BackendSettings(std::make_unique<FBackendSettings>())
	, SocketManager(std::make_unique<FSocketManager>())
	, ConversationsManager(std::make_unique<FConversationsManager>())
	, bIsSSLEnabled(false)
{
	// Collect Database settings
	FDataBaseSettings::Initialize();
}

void FProjectEngine::Init()
{
	/** We do not need SDL input in server */
	DisableInput();

	FEngine::Init();

	LOG_DEBUG("Server init");

	UserManager = std::make_unique<FUserManager>();
	BackendSettings->LoadBackendSettings();
	AbuseProtectionPtr = std::make_unique<FAbuseProtection>(BackendSettings.get());
	DefaultHeadersCache = GetDefaultHeaders();

	std::shared_ptr<FIniObject> ServerSettingsIni = BackendSettings->GetBackendSettingsIni();
	if (ServerSettingsIni->DoesIniExist())
	{
		// Get domain name from ini
		{
			const FIniField DomainField = ServerSettingsIni->FindFieldByName("Domain");
			if (DomainField.IsValid())
			{
				DomainName = DomainField.GetValueAsString();
			}
		}

		// Read all backend addresses from ini
		{
			FIniField BackendAddressField = ServerSettingsIni->FindFieldByName("BackendAddress" + std::to_string(1));
			for (int32 i = 2; i < 128 && BackendAddressField.IsValid(); i++)
			{
				OriginWhitelist.Push(BackendAddressField.GetValueAsString());
				BackendAddressField = ServerSettingsIni->FindFieldByName("BackendAddress" + std::to_string(i));
			}
		}

#if DEBUG
		{
			// Debug domain
			FIniField DebugDomainField = ServerSettingsIni->FindFieldByName("DebugDomain");

			OriginWhitelist.InsertAt(0 ,"http://" + DebugDomainField.GetValueAsString());
			DomainName = DebugDomainField.GetValueAsString();
		}
#endif

		InitBasicSetup();

		LOG_DEBUG("Created api test");

		InitUsersSetup();

		LOG_DEBUG("Created api user");

		UserManager->Init();

		// HTTP/REST crow server
		StartServer(ServerSettingsIni);

		// Socket
		const FIniField PortWSField = ServerSettingsIni->FindFieldByName("PortWS");
		if (PortWSField.IsValid())
		{
			SocketManager->CreateSockets(PortWSField.GetValueAsInt(), bIsSSLEnabled, KeyFilePath, CertFilePath);
		}
	}
	else
	{
		LOG_ERROR("Ini is missing, API will not work.");
	}
}

void FProjectEngine::PostSecondTick()
{
	FEngine::PostSecondTick();

	UserManager->PostSecondTick();
}

void FProjectEngine::InitBasicSetup()
{
	// Most common address to check if it works
	CROW_ROUTE(CrowApp, "/")([this]()
		{
			return CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "Crow C++ API Server is running."} });
		});

	// Route for testing if api works
	CROW_ROUTE(CrowApp, "/api/v1/test")([this]()
		{
			return CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "API is working."} });
		});
}

void FProjectEngine::InitUsersSetup()
{
	CROW_ROUTE(CrowApp, "/api/v1/users/register")
		.methods("POST"_method, "OPTIONS"_method)
		([this] (const crow::request& req)
		{
			crow::response OutResponse = CreateResponse(400, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

			// Get IP address
			const std::string& ClientIP = req.remote_ip_address;

			const crow::json::rvalue JsonData = crow::json::load(req.body);
			if (JsonData)
			{
				const std::string UserName = JsonData["username"].s();
				const std::string UserPassword = JsonData["password"].s();
				const std::string EMail = JsonData["email"].s();

				const ERegisterUserStatus RegisterStatus = UserManager->RegisterUser(UserName, UserPassword, EMail);

				switch (RegisterStatus)
				{
					case ERegisterUserStatus::Unknown:
					{
						OutResponse = CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Registration failed. User may already exist or invalid input."} });

						break;
					}
					case ERegisterUserStatus::Successful:
					{
						AbuseProtectionPtr->AddRateLimitedAttempt(ClientIP);

						OutResponse = CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "User registered successfully."} });

						break;
					}
					case ERegisterUserStatus::MailTaken:
					{
						OutResponse = CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Registration failed. User may already exist or invalid input."} });

						break;
					}
					case ERegisterUserStatus::PasswordToWeak:
					{
						OutResponse = CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Registration failed. Password too weak."} });

						break;
					}
					case ERegisterUserStatus::DataBaseInsertFailed:
					{
						LOG_ERROR("ERegisterUserStatus::DataBaseInsertFailed:");

						OutResponse = CreateResponse(crow::status::INTERNAL_SERVER_ERROR, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Sorry."} });

						break;
					}
					case ERegisterUserStatus::DataBaseConnectionFailed:
					{
						LOG_ERROR("ERegisterUserStatus::DataBaseConnectionFailed:");

						OutResponse = CreateResponse(crow::status::INTERNAL_SERVER_ERROR, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Sorry."} });

						break;
					}
					default:
					{
						LOG_ERROR("RegisterStatus unknown case");
					}
				}
			}
			else
			{
				OutResponse = CreateResponse(429, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message","Too many requests."} });
			}

			return OutResponse;
		});

	CROW_ROUTE(CrowApp, "/api/v1/users/login")
		.methods("POST"_method, "OPTIONS"_method)
		([this](const crow::request& req)
		{
			crow::response OutResponse = CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

			// Get IP address
			const std::string& ClientIP = req.remote_ip_address;

			const crow::json::rvalue JsonData = crow::json::load(req.body);
			if (JsonData)
			{
				const std::string UserEmail = JsonData["email"].s();
				const std::string UserPassword = JsonData["password"].s();

				std::string OutSessionToken;
				const ELoginStatus LoginStatus = UserManager->LoginUser(UserEmail, UserPassword, OutSessionToken);

				if (!OutSessionToken.empty())
				{
					switch (LoginStatus)
					{
						case ELoginStatus::Unknown:
						{
							OutResponse = CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "User login successful!"}, { "message", "unknown issue" } });

							break;
						}
						case ELoginStatus::Successful:
						{
							OutResponse = CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "User login successful!"} });
							AddCookies(OutResponse, OutSessionToken);

							break;
						}
						case ELoginStatus::SessionAlreadyExist:
						{
							OutResponse = CreateResponse(crow::status::NO_CONTENT, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Session already exists!"} });

							break;
						}
						case ELoginStatus::IncorrectCredentialsOrUserDoesNotExist:
						{
							OutResponse = CreateResponse(crow::status::FORBIDDEN, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Wrong credentials!"} });

							break;
						}
						default:
						{
							LOG_ERROR("LoginStatus unknown value!");
						}
					}
				}
				else
				{
					OutResponse = CreateResponse(crow::status::INTERNAL_SERVER_ERROR, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Unable to generate session."} });
				}
			}

			return OutResponse;
		});

	CROW_ROUTE(CrowApp, "/api/v1/users/verify")
		.methods("POST"_method, "OPTIONS"_method)
		([this](const crow::request& req)
			{
				crow::response OutResponse = CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

				const std::string_view CookieHeader = req.get_header_value("Cookie");
				const std::string Token = FCookieHelper::GetCookieValue(CookieHeader, "auth_token");

				// Get IP address
				const std::string& ClientIP = req.remote_ip_address;

				if (UserManager->VerifyToken(Token))
				{
					OutResponse = CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Token correct."} });
				}
				else
				{
					OutResponse = CreateResponse(crow::status::UNAUTHORIZED, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Token incorrect."} });

					AbuseProtectionPtr->AddRateLimitedAttempt(ClientIP);
				}

				return OutResponse;
			});

	CROW_ROUTE(CrowApp, "/api/v1/users/refresh")
		.methods("POST"_method, "OPTIONS"_method)
		([this](const crow::request& req)
			{
				crow::response OutResponse = CreateResponse(400, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

				const std::string_view CookieHeader = req.get_header_value("Cookie");
				const std::string Token = FCookieHelper::GetCookieValue(CookieHeader, "auth_token");

				if (!Token.empty())
				{
					if (UserManager->RefreshSessionToken(Token))
					{
						OutResponse = CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Token has new refreshed."} });
					}
					else
					{
						OutResponse = CreateResponse(crow::status::UNAUTHORIZED, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Token not found."} });
					}

					// Get IP address
					const std::string& ClientIP = req.remote_ip_address;

					AbuseProtectionPtr->AddRateLimitedAttempt(ClientIP);
				}

				return OutResponse;
			});

	CROW_ROUTE(CrowApp, "/api/v1/users/logout")
		.methods("POST"_method, "OPTIONS"_method)
		([this](const crow::request& req)
		{
			crow::response OutResponse = CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Invalid JSON."} });

			const crow::json::rvalue JsonData = crow::json::load(req.body);
			if (JsonData)
			{
				const std::string SessionToken = JsonData["token"].s();

				const bool bSuccessfullyLoggedOut = UserManager->Logout(SessionToken);
				if (bSuccessfullyLoggedOut)
				{
					OutResponse = CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "Session terminated!"} });
				}
				else
				{
					OutResponse = CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error }, { "message", "Can not log out."} });
				}
			}

			return OutResponse;
		});
}

void FProjectEngine::InitCommunicatorSetup()
{
	/*
	CROW_ROUTE(CrowApp, "/api/v1/comm/addchat")
		.methods("POST"_method, "OPTIONS"_method)
		([this](const crow::request& req)
		{
			crow::response OutResponse = CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error },
				{ FPredefinedMessages::Message::Message, "Invalid JSON."} });

			const crow::json::rvalue JsonData = crow::json::load(req.body);
			if (JsonData)
			{
				// Get IP address
				const std::string& ClientIP = req.remote_ip_address;

				const std::string_view CookieHeader = req.get_header_value("Cookie");
				const std::string Token = FCookieHelper::GetCookieValue(CookieHeader, "auth_token");

				if (!Token.empty())
				{
					if (UserManager->VerifyToken(Token))
					{
						const Uint64 CurrentUserId = UserManager->GetIdFromToken(Token);
						const Uint64 OtherUserId = static_cast<Uint64>(JsonData["other_user_id"].i());

						if (CurrentUserId > 0 && OtherUserId > 0)
						{
							FDataBaseConnect Connect;
							if (Connect.IsConnected())
							{
								// Get database connection session
								soci::session& DataBaseSession = Connect.GetSession();

								// Check if 1-on-1 conversation exists between two users
								int64_t conversationId = 0;
								soci::indicator ind;

								// cp1 and cp2 are table aliases (shortcuts)
								// cp1 = conversation_participants (first copy)
								// cp2 = conversation_participants (second copy)
								DataBaseSession << "SELECT cp1.ConversationId "
									"FROM conversation_participants cp1 "
									"JOIN conversation_participants cp2 "
									"  ON cp1.ConversationId = cp2.ConversationId "
									"JOIN conversations c "
									"  ON c.ConversationId = cp1.ConversationId "
									"WHERE cp1.UserId = :user1 "
									"  AND cp2.UserId = :user2 "
									"  AND c.IsGroup = 0 "
									"LIMIT 1",
									soci::into(conversationId, ind),
									soci::use(CurrentUserId),
									soci::use(OtherUserId);

								// Check result
								if (ind == soci::i_ok)
								{
									// Conversation exists
									std::cout << "Found conversation: " << conversationId << std::endl;
								}
								else
								{
									// Conversation doesn't exist
									std::cout << "No conversation found" << std::endl;
								}
							}
						}
					}
					else
					{
						AbuseProtectionPtr->AddRateLimitedAttempt(ClientIP);

						OutResponse = CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error } });
					}
				}
				else
				{
					OutResponse = CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error } });
				}
			}

			return OutResponse;
		});

	CROW_ROUTE(CrowApp, "/api/v1/comm/searchusers")
		.methods("POST"_method, "OPTIONS"_method)
		([this](const crow::request& req)
		{
			crow::response OutResponse = CreateResponse(crow::status::BAD_REQUEST, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Error },
				{ FPredefinedMessages::Message::Message, "Invalid JSON."} });

			const crow::json::rvalue JsonData = crow::json::load(req.body);
			if (JsonData)
			{
				
			}
		});
		*/
}

void FProjectEngine::StartServer(const std::shared_ptr<FIniObject>& ServerSettingsIni)
{
	// Find port in settings
	constexpr uint16 ServerPortDefault = 8080;

	int32 ServerPort;
	bIsSSLEnabled = false;
	bool bDoesServerSettingsExist = ServerSettingsIni->DoesIniExist();
	if (bDoesServerSettingsExist)
	{
		const FIniField ServerPortField = ServerSettingsIni->FindFieldByName("Port");
		if (ServerPortField.IsValid())
		{
			ServerPort = ServerPortField.GetValueAsInt();
		}

		const FIniField EnableSSLField = ServerSettingsIni->FindFieldByName("EnableSSL");
		if (EnableSSLField.IsValid())
		{
			bIsSSLEnabled = EnableSSLField.GetValueAsBool();
		}

		if (bIsSSLEnabled)
		{
			const FAssetsManager* AssetsManager = FGlobalDefines::GEngine->GetAssetsManager();
			const std::string ConfigPathAbsolute = AssetsManager->ConvertRelativeToFullPath(AssetsManager->GetConfigPathRelative());

			const FIniField SSLKeyField = ServerSettingsIni->FindFieldByName("SSLKey");
			if (SSLKeyField.IsValid())
			{
				KeyFilePath = ConfigPathAbsolute + AssetsManager->GetPlatformSlash() + SSLKeyField.GetValueAsString();
			}

			const FIniField SSLCertField = ServerSettingsIni->FindFieldByName("SSLCert");
			if (SSLCertField.IsValid())
			{
				CertFilePath = ConfigPathAbsolute + AssetsManager->GetPlatformSlash() + SSLCertField.GetValueAsString();
			}
		}
	}
	else
	{
		ServerPort = ServerPortDefault;
	}

	if (bDoesServerSettingsExist && bIsSSLEnabled)
	{
		LOG_INFO("Server will start with SSL");

		if (FFileSystem::File::Exists(CertFilePath) && FFileSystem::File::Exists(KeyFilePath))
		{
			CrowAppFutureAsync = CrowApp.port(static_cast<Uint16>(ServerPort))
				.ssl_file(CertFilePath.c_str(), KeyFilePath.c_str())
				.multithreaded()
				.run_async();
		}
		else
		{
			LOG_ERROR("Backend will NOT start.");
			LOG_ERROR("Attempted to start crow with SSL but Key or Cert files are missing\n.");


			LOG_INFO("Expected paths:");
			LOG_INFO("CertFilePath: " << CertFilePath);
			LOG_INFO("KeyFilePath: " << KeyFilePath);

			LOG_INFO("\nTo generate for testing use bat script in Assets.");
		}
	}
	else
	{
		LOG_INFO("Server will start without SSL");

		CrowAppFutureAsync = CrowApp.port(static_cast<uint16>(ServerPort))
			.multithreaded()
			.run_async();
	}
}

void FProjectEngine::PreExit()
{
	FEngine::PreExit();

	CrowApp.stop();

	//CrowAppFutureAsync.wait();

	LOG_INFO("Stopped crow");
}

void FProjectEngine::AddHeaders(crow::response& CurrentResponse, const CUnorderedMap<std::string, std::string>& HeaderNameToValueMap)
{
	for (const std::pair<const std::string, std::string>& Value : HeaderNameToValueMap)
	{
		CurrentResponse.add_header(Value.first, Value.second);
	}
}

void FProjectEngine::AddCookies(crow::response& CurrentResponse, const std::string& AuthToken)
{
	if (bIsSSLEnabled)
	{
		// HTTP
		CurrentResponse.add_header("Set-Cookie", "auth_token=" + AuthToken + "; Domain=" + DomainName + "; Path=/; HttpOnly; Secure; SameSite=Strict; Max-Age=86400");
	}
	else
	{
		// HTTPS
		CurrentResponse.add_header("Set-Cookie", "auth_token=" + AuthToken + "; Domain=" + DomainName + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=86400");
	}
}

CUnorderedMap<std::string, std::string> FProjectEngine::GetDefaultHeaders() const
{
	CUnorderedMap<std::string, std::string> OutDefaultHeaders = AbuseProtectionPtr->GetCORHeaders();

	return OutDefaultHeaders;
}

crow::response FProjectEngine::CreateResponse(const int ResponseCode, const CMap<std::string, std::string>& JsonFields) const
{
	crow::json::wvalue response;

	for (const std::pair<const std::string, std::string>& JsonField : JsonFields)
	{
		response[JsonField.first] = JsonField.second;
	}

	return crow::response(ResponseCode, response);
}
