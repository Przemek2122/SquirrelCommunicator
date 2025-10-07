#include <gtest/gtest.h>
#include "asio.hpp"
#include <asio/ssl.hpp>
#include "Public/ProjectEngine.h"

int main(int argc, char** argv)
{
    FEngineManager EngineManager;

    std::thread EngineThread([&]()
    {
        EngineManager.EngineClass.Set<FProjectEngine>();
        EngineManager.Start(argc, argv);
    });

    // Wait for EngineThread as we need server for tests
    for (int32 i = 0; i < 1024; ++i)
    {
        FEngine* Engine = FEngineManager::Get();
        if (Engine != nullptr)
        {
            // Engine initialised
            break;
        }
        else
        {
	        // Wait
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    // Wait for crow to initialize
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Run test
    ::testing::InitGoogleTest(&argc, argv);
    const int RunOutput = RUN_ALL_TESTS();

    // Shut down engine
    FProjectEngine* ProjectEngine = FEngineManager::Get<FProjectEngine>();
    if (ProjectEngine != nullptr)
    {
        ProjectEngine->RequestExit();
    }
    else
    {
        LOG_ERROR("Engine init failed");
    }

    // Join threads
    EngineThread.join();

    return RunOutput;
}

TEST(BackendTest, CrowValidate)
{
    FProjectEngine* ProjectEngine = FEngineManager::Get<FProjectEngine>();
    EXPECT_TRUE(ProjectEngine != nullptr);
    if (ProjectEngine != nullptr)
    {
	    crow::SimpleApp& CrowApp = ProjectEngine->GetCrowApp();

        CrowApp.validate();
    }
}

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

const FConnectionData LocalConnectionData("127.0.0.1", "8080");

std::string SendRequest(const FConnectionData& InConnectionData, const std::string& Request)
{
    using asio::ip::tcp;
    asio::io_context io_context;

    // Setup SSL context
    asio::ssl::context ssl_ctx(asio::ssl::context::tlsv12_client);
    ssl_ctx.set_default_verify_paths();
    // For self-signed certs in tests, disable verification
    ssl_ctx.set_verify_mode(asio::ssl::verify_none);

    // Connect to server
    tcp::resolver resolver(io_context);
    asio::ip::basic_resolver_results<tcp> endpoints = resolver.resolve(InConnectionData.HostName, InConnectionData.Port);

    asio::ssl::stream<tcp::socket> ssl_socket(io_context, ssl_ctx);

    asio::error_code connect_ec;
    asio::connect(ssl_socket.lowest_layer(), endpoints, connect_ec);

    if (connect_ec)
    {
        LOG_ERROR("Failed to connect: " << connect_ec.message());
        return "";
    }

    // SSL handshake
    asio::error_code handshake_ec;
    ssl_socket.handshake(asio::ssl::stream_base::client, handshake_ec);

    if (handshake_ec)
    {
        LOG_ERROR("SSL handshake failed: " << handshake_ec.message());
        return "";
    }

    // Send request
    asio::error_code write_ec;
    asio::write(ssl_socket, asio::buffer(Request), write_ec);

    if (write_ec)
    {
        LOG_ERROR("Failed to write request: " << write_ec.message());
        return "";
    }

    // Read response
    asio::streambuf response;
    asio::error_code ec;
    size_t bytes_read = asio::read_until(ssl_socket, response, "\r\n\r\n", ec);

    std::string ResponseStr;
    if (ec)
    {
        LOG_ERROR("Failed to read response: " << ec.message());
    }
    else if (bytes_read == 0 || response.size() == 0)
    {
        LOG_ERROR("Empty response received");
    }
    else
    {
        ResponseStr = { std::istreambuf_iterator<char>(&response), {} };
    }

    return ResponseStr;
}

TEST(BackendTest, ConnectionTest)
{
    const std::string Request =
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n\r\n";

    const std::string Response = SendRequest(LocalConnectionData, Request);

    LOG_INFO("Response:" << Response);

    EXPECT_TRUE(Response.find("Crow C++ API Server is running.") != std::string::npos);
}
