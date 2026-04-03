#include "Rest/CrowAppMiddleware.h"
#include "ProjectEngine.h"
#include "AbuseProtection/AbuseProtection.h"

void FCrowAppMiddleware::before_handle(crow::request& Req, crow::response& Res, context& Ctx)
{
	const FProjectEngine* ProjectEngine = static_cast<FProjectEngine*>(FGlobalDefines::GEngine);
	const FAbuseProtection* AbuseProtection = ProjectEngine->GetAbuseProtection();

	// Get IP address
	const std::string& ClientIP = Req.remote_ip_address;

	if (!AbuseProtection->IsAddressBlocked(ClientIP))
	{
		// Options support
		if (Req.method == crow::HTTPMethod::Options)
		{
			Res.code = 204;
			Res.end();
		}
	}
	else
	{
		// Block due to Too Many Requests
		Res.code = crow::status::TOO_MANY_REQUESTS;
		Res.body = R"({"error":"Rate limit exceeded"})";
		Res.end();
	}
}

void FCrowAppMiddleware::after_handle(crow::request& Req, crow::response& Res, context& Ctx)
{
	static const std::string AccessControlAllowOriginHeaderName = "Access-Control-Allow-Origin";

	const std::string Origin = Req.get_header_value("Origin");
	FProjectEngine* ProjectEngine = static_cast<FProjectEngine*>(FGlobalDefines::GEngine);
	const CArray<std::string>& Whitelist = ProjectEngine->GetOriginWhitelist();

	ProjectEngine->AddHeaders(Res, ProjectEngine->GetDefaultHeadersCache());

	if (Whitelist.Size() && Whitelist.Contains(Origin))
	{
		ProjectEngine->AddHeaders(Res, { { AccessControlAllowOriginHeaderName, Origin } });
	}
	else
	{
		ProjectEngine->AddHeaders(Res, { { AccessControlAllowOriginHeaderName, Whitelist[0]} });
	}
}

