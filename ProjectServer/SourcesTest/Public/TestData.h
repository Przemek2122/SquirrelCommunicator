#pragma once

struct FConnectionData
{
	FConnectionData(const std::string& InHostName, const std::string& InPort)
		: HostName(InHostName)
		, Port(InPort)
	{
	}

	std::string HostName;
	std::string Port;
};

struct FUserSampleData
{
	FUserSampleData(std::string InUserName, std::string InPassword, std::string InEMail)
		: UserName(std::move(InUserName)),
		Password(std::move(InPassword)),
		EMail(std::move(InEMail))
	{
	}

	std::string UserName;
	std::string Password;
	std::string EMail;
};

struct FHttpResponse
{
	FHttpResponse(int32 InStatusCode, const std::map<std::string, std::string>& InHeaders, std::string InBody = std::string())
		: StatusCode(InStatusCode),
		Headers(InHeaders),
		Body(std::move(InBody))
	{
	}

	int32 StatusCode;
	std::map<std::string, std::string> Headers;
	std::string Body;
};
