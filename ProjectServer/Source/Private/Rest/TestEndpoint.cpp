#include "Rest/TestEndpoint.h"
#include "PredefinedMessages.h"
#include "Rest/CrowUtils.h"

FTestEndpoint::FTestEndpoint(FProjectEngine* InProjectEngine)
	: FCrowAppEndpoint(InProjectEngine)
{
}

void FTestEndpoint::RegisterRoutes(crow::App<FCrowAppMiddleware>& App)
{
	// Most common address to check if it works
	CROW_ROUTE(App, "/")([]()
	{
		return FCrowUtils::CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "Squirrel communicator is running."} });
	});

	// Route for health check
	CROW_ROUTE(App, "/health")([]()
	{
		return FCrowUtils::CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "API is working."} });
	});

	// Route for checking if api is working
	CROW_ROUTE(App, "/api/v1/is_live")([]()
	{
		return FCrowUtils::CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "true"} });
	});

	// Route for testing if api works
	CROW_ROUTE(App, "/api/v1/test")([]()
	{
		return FCrowUtils::CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "API is working."} });
	});

	FCrowAppEndpoint::RegisterRoutes(App);
}
