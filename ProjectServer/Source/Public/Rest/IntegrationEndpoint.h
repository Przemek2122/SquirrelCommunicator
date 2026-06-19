#pragma once

#include "EngineCompat.h"
#include "CrowAppEndpoint.h"

class FUser;
enum class ELoginStatus : Uint8;

class FIntegrationEndpoint : public FCrowAppEndpoint
{
public:
	FIntegrationEndpoint(FProjectEngine* InProjectEngine = nullptr);

	void RegisterRoutes(crow::App<FCrowAppMiddleware>& App) override;

	std::shared_ptr<FUser> RegisterIntegration(crow::response &OutResponse, const std::string &Mail, const std::string &Name) const;
	void HandleLoginCase(crow::response& OutResponse, ELoginStatus LoginStatus, const std::string& OutSessionToken, const std::string& ClientIP) const;

};

