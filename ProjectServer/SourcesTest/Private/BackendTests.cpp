#include "BackendTests.h"
#include "ProjectEngine.h"

std::string FBackendHelper::BuildHttpRequest(const std::string& Method, const std::string& Path,
	const std::string& Host, const std::map<std::string, std::string>& Headers, const std::string& Body)
{
	std::ostringstream Request;

	// Request line
	Request << Method << " " << Path << " HTTP/1.1\r\n";
	Request << "Host: " << Host << "\r\n";

	// Add body length if body exists
	if (!Body.empty())
	{
		Request << "Content-Length: " << Body.size() << "\r\n";
	}

	// Add custom headers
	for (const auto& [Key, Value] : Headers)
	{
		Request << Key << ": " << Value << "\r\n";
	}

	// End of headers
	Request << "\r\n";

	// Add body
	if (!Body.empty())
	{
		Request << Body;
	}

	return Request.str();
}

std::string FBackendHelper::SendRequest(const FConnectionData& InConnectionData, const std::string& Request)
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

TEST_F(FBackendTest, CrowValidate)
{
	FProjectEngine* ProjectEngine = FEngineManager::Get<FProjectEngine>();
	EXPECT_TRUE(ProjectEngine != nullptr);
	if (ProjectEngine != nullptr)
	{
		crow::SimpleApp& CrowApp = ProjectEngine->GetCrowApp();

		CrowApp.validate();
	}
}

TEST_F(FBackendTest, Connection)
{
	const std::map<std::string, std::string> Headers;

	const std::string Body = "";

	const std::string Request = FBackendHelper::BuildHttpRequest(
		"GET",
		"/",
		LocalConnectionData.HostName,
		Headers,
		Body
	);

	const std::string Response = FBackendHelper::SendRequest(LocalConnectionData, Request);

	LOG_INFO("Response:" << Response);

	EXPECT_TRUE(Response.find("Crow C++ API Server is running.") != std::string::npos);
}

TEST_F(FBackendTest, RegisterNewUser)
{
	const std::map<std::string, std::string> Headers{
		{"Content-Type", "application/json"},
	};

	const std::string Body = "{\n"
		"\t\"username\":\"" + UserData.UserName + "\",\n"
		"\t\"password\":\"" + UserData.Password + "\",\n"
		"\t\"email\":\"" + UserData.EMail + "\"\n"
		"}";

	const std::string Request = FBackendHelper::BuildHttpRequest(
		"POST",
		"/api/v1/users/register",
		LocalConnectionData.HostName,
		Headers,
		Body
	);

	const std::string Response = FBackendHelper::SendRequest(LocalConnectionData, Request);

	LOG_INFO("Response:" << Response);

	EXPECT_TRUE(Response.find("User registered successfully") != std::string::npos);
}

TEST_F(FBackendTest, LoginNewUser)
{
	const std::map<std::string, std::string> Headers{
		{"Content-Type", "application/json"},
	};

	const std::string Body = "{\n"
		"\t\"username\":\"" + UserData.UserName + "\",\n"
		"\t\"password\":\"" + UserData.Password + "\"\n"
		"}";

	const std::string Request = FBackendHelper::BuildHttpRequest(
		"POST",
		"/api/v1/users/login",
		LocalConnectionData.HostName,
		Headers,
		Body
	);

	const std::string Response = FBackendHelper::SendRequest(LocalConnectionData, Request);

	LOG_INFO("Response:" << Response);

	EXPECT_TRUE(Response.find("User login successful") != std::string::npos);
}

TEST_F(FBackendTest, VerifyToken)
{

}
