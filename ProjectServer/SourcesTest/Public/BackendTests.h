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

    // Parses raw HTTP response into structured data
	static FHttpResponse ParseHttpResponse(const std::string& Response)
    {
        FHttpResponse Result = FHttpResponse(-1, { });
        std::istringstream Stream(Response);
        std::string Line;

        // Parse status line: HTTP/1.1 200 OK
        std::getline(Stream, Line);
        std::istringstream StatusLine(Line);
        std::string HttpVersion;
        StatusLine >> HttpVersion >> Result.StatusCode;

        // Parse headers
        while (std::getline(Stream, Line) && Line != "\r")
        {
            if (Line.back() == '\r') Line.pop_back();

            size_t Pos = Line.find(':');
            if (Pos != std::string::npos)
            {
                std::string Key = Line.substr(0, Pos);
                std::string Value = Line.substr(Pos + 2); // Skip ": "
                Result.Headers[Key] = Value;
            }
        }

        // Remaining is body
        std::ostringstream BodyStream;
        BodyStream << Stream.rdbuf();
        Result.Body = BodyStream.str();

        return Result;
    }
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

    /** Token set when log in successful for next tests */
    inline static std::string Token;
};
