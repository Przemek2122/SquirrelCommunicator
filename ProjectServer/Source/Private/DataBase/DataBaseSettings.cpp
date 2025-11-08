#include "DataBase/DataBaseSettings.h"

static FDataBaseConnectionData DataBaseConnectionData;
static std::string ConnectionString;

FDataBaseConnectionData::FDataBaseConnectionData()
{
}

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
	}

	return Variable ? Variable : "localhost";
}

std::string FDataBaseSettings::GetEnvPort()
{
	const char* Variable = std::getenv("SQRLL_COMM_DB_PORT");
	return Variable ? Variable : "3306";
}

std::string FDataBaseSettings::GetEnvDataBaseName()
{
	const char* Variable = std::getenv("SQRLL_COMM_DB_DBNAME");
	return Variable ? Variable : "sqrllapi";
}

std::string FDataBaseSettings::GetEnvUser()
{
	const char* Variable = std::getenv("SQRLL_COMM_DB_USER");
	return Variable ? Variable : "commapisqrlluser";
}

std::string FDataBaseSettings::GetEnvPassword()
{
	const char* Variable = std::getenv("SQRLL_COMM_DB_PASSWORD");
	if (Variable == nullptr)
	{
		LOG_ERROR("SQRLL_COMM_DB_PASSWORD not set");
	}

	return Variable ? Variable : "error";
}
