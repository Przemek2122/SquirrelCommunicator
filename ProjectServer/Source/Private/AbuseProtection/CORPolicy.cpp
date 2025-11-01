#include "ProjectEngine.h"
#include "AbuseProtection/CORPolicy.h"

FCORPolicy::FCORPolicy()
	: CORHeaders({
		{ "Access-Control-Allow-Origin", "*" }, // Allow everything is bad but for now it should be ok
		{ "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS" },
		{ "Access-Control-Allow-Headers", "Content-Type, Authorization" }
	})
{
}
