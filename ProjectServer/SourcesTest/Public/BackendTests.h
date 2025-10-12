#pragma once

#include "asio.hpp"
#include <asio/ssl.hpp>
#include <gtest/gtest.h>
#include "TestData.h"

class FBackendHelper
{
public:
	/**
	 * @param Method // GET, POST, etc.
	 * @param Path // /api/endpoint
	 * @param Host // example.com
	 * @param Headers // Custom headers
	 * @param Body // Request body (optional)
	 * @return formatted request for SendRequest
	 */
	static std::string BuildHttpRequest(
        const std::string& Method,           
        const std::string& Path,             
        const std::string& Host,             
        const std::map<std::string, std::string>& Headers,  
        const std::string& Body = ""
    );

	/**
	 * Function for sending request to a server
	 * @param InConnectionData Server HostName and Port
	 * @param Request Formatted data, for easier use check FBackendHelper::BuildHttpRequest
	 * @return response from server
	 */
	static std::string SendRequest(const FConnectionData& InConnectionData, const std::string& Request);
};

class FBackendTest : public ::testing::Test
{
public:
	FBackendTest()
		: LocalConnectionData(FConnectionData("127.0.0.1", "8080"))
		, UserData(FUserSampleData("myUser", "mySecret", "email_secret@secretemail.xyz"))
	{
	}

protected:
	void SetUp() override
    {
    }

    void TearDown() override
    {
    }

    const FConnectionData LocalConnectionData;
    const FUserSampleData UserData;
};
