
#include "Logger/Logger.h"
#include "Rest/CrowAppEndpoint.h"

FCrowAppEndpoint::FCrowAppEndpoint(FProjectEngine* InProjectEngine)
	: ProjectEngine(InProjectEngine)
{
}

void FCrowAppEndpoint::RegisterRoutes(crow::App<FCrowAppMiddleware>& App)
{
	LOG_DEBUG("Register routes.");
}
