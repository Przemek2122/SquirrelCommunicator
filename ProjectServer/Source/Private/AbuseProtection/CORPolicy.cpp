#include "ProjectEngine.h"
#include "AbuseProtection/CORPolicy.h"

FCORPolicy::FCORPolicy()
	: CORHeaders({
		{ "Access-Control-Allow-Origin", "*" } // Allow everything is bad but for now it should be ok
	})
{
}
