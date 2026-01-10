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
	CROW_ROUTE(App, "/")([this]()
		{
			return FCrowUtils::CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "Crow C++ API Server is running."} });
		});

	// Route for testing if api works
	CROW_ROUTE(App, "/api/v1/test")([this]()
		{
			return FCrowUtils::CreateResponse(crow::status::OK, { { FPredefinedMessages::Status::Name, FPredefinedMessages::Status::Success }, { "message", "API is working."} });
		});

	FCrowAppEndpoint::RegisterRoutes(App);
}
