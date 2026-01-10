#include "Rest/IntegrationEndpoint.h"

FIntegrationEndpoint::FIntegrationEndpoint(FProjectEngine* InProjectEngine)
	: FCrowAppEndpoint(InProjectEngine)
{
}

void FIntegrationEndpoint::RegisterRoutes(crow::App<FCrowAppMiddleware>& App)
{


	FCrowAppEndpoint::RegisterRoutes(App);
}
