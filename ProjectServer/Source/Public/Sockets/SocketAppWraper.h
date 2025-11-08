#pragma once

#include <variant>
#include "uWebSockets/App.h"

class FSocketAppWrapper
{
public:
    FSocketAppWrapper(bool bInUseSSL, const std::string& InKeyPath, const std::string& InCertPath)
        : bUseSSL(bInUseSSL)
    {
        if (bInUseSSL)
        {
            SSLApp = std::make_unique<uWS::SSLApp>(uWS::SocketContextOptions{
                .key_file_name = InKeyPath.c_str(),
                .cert_file_name = InCertPath.c_str()
                });
        }
        else
        {
            NoSSLApp = std::make_unique<uWS::App>(); // No temporary
        }
    }

    // WebSocket setup with ssl
    template<typename PerSocketData>
    FSocketAppWrapper& wsssl(const std::string& pattern, uWS::TemplatedApp<true>::WebSocketBehavior<PerSocketData> BehaviorWithSSL)
    {
		SSLApp->ws<PerSocketData>(pattern, std::move(BehaviorWithSSL));

		return *this;
    }

    // WebSocket setup WITHOUT ssl
    template<typename PerSocketData>
    FSocketAppWrapper& wssslno(const std::string& pattern, uWS::TemplatedApp<false>::WebSocketBehavior<PerSocketData> BehaviourNoSSL)
    {
        NoSSLApp->ws<PerSocketData>(pattern, std::move(BehaviourNoSSL));

        return *this;
    }

    // Listen
    FSocketAppWrapper& listen(int port, auto&& handler)
    {
        if (bUseSSL)
        {
            SSLApp->listen(port, std::forward<decltype(handler)>(handler));
        }
        else
        {
            NoSSLApp->listen(port, std::forward<decltype(handler)>(handler));
        }

        return *this;
    }

    void Run()
    {
        if (bUseSSL)
        {
            SSLApp->run();
        }
        else
        {
            NoSSLApp->run();
        }
    }

    bool IsUsingSSL() const { return bUseSSL; }

private:
    std::unique_ptr<uWS::App> NoSSLApp;
    std::unique_ptr<uWS::SSLApp> SSLApp;
    bool bUseSSL;
};