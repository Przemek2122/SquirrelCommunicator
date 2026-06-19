#include "Logger/Logger.h"
#include "EngineCompat.h"
#include "DataBase/DataBaseSettings.h"

static FDataBaseConnectionData DataBaseConnectionData;
static std::string ConnectionString;

FDataBaseConnectionData::FDataBaseConnectionData() = default;

const FDataBaseConnectionData& FDataBaseSettings::GetDataBaseConnectionData()
{
	return DataBaseConnectionData;
}

const std::string& FDataBaseSettings::GetConnectionString()
{
	return ConnectionString;
}

void FDataBaseSettings::Initialize()
{
	DataBaseConnectionData.Host = GetEnvHost();
	DataBaseConnectionData.Port = GetEnvPort();
	DataBaseConnectionData.DataBaseName = GetEnvDataBaseName();
	DataBaseConnectionData.UserName = GetEnvUser();
	DataBaseConnectionData.Password = GetEnvPassword();

	ConnectionString = "host=";
	ConnectionString += DataBaseConnectionData.Host;

	ConnectionString += " port=";
	ConnectionString += DataBaseConnectionData.Port;

	ConnectionString += " dbname=";
	ConnectionString += DataBaseConnectionData.DataBaseName;

	ConnectionString += " user=";
	ConnectionString += DataBaseConnectionData.UserName;

	ConnectionString += " password=";
	ConnectionString += DataBaseConnectionData.Password;

}

std::string FDataBaseSettings::GetEnvHost()
{
	const char* Variable = std::getenv("SQRLL_COMM_DB_HOST");
	if (Variable == nullptr)
	{
		LOG_INFO("SQRLL_COMM_DB_HOST empty, defaulting to localhost.");
		Variable = "localhost";
	}

	return Variable;
}

std::string FDataBaseSettings::GetEnvPort()
{
	const char* Variable = std::getenv("SQRLL_COMM_DB_PORT");
	if (Variable == nullptr)
	{
		Variable = "3306";
	}

	return Variable;
}

std::string FDataBaseSettings::GetEnvDataBaseName()
{
	const char* Variable = std::getenv("SQRLL_COMM_DB_DBNAME");
	if (Variable == nullptr)
	{
		Variable = "sqrllapitest";
	}

	return Variable;
}

std::string FDataBaseSettings::GetEnvUser()
{
	const char* Variable = std::getenv("SQRLL_COMM_DB_USER");
	if (Variable == nullptr)
	{
		Variable = "commapisqrllusertest";
	}

	return Variable;
}

std::string FDataBaseSettings::GetEnvPassword()
{
	const char* Variable = std::getenv("SQRLL_COMM_DB_PASSWORD");
	if (Variable == nullptr)
	{
		LOG_ERROR("SQRLL_COMM_DB_PASSWORD not set");

		// You are missing a default password, you can set it by
		// Setting it in the env. for os (recommended to do not by mistake push to repo)
		ENSURE_VALID(false);
	}

	return Variable ? Variable : "error";
}
