#include "ProjectEngine.h"
#include "AbuseProtection/CORPolicy.h"

FCORPolicy::FCORPolicy()
	: CORHeaders({
		{ "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS" },
		{ "Access-Control-Allow-Headers", "Content-Type, Authorization" },
		{ "Access-Control-Allow-Credentials", "true" }
	})
{
}
