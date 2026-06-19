// WebSocket App Wrapper - Standalone version
// Copied from Engine's Misc/WebSockets/AppWrapper.h
// Wraps uWS::App and uWS::SSLApp to hide SSL template complexity

#pragma once

#include <variant>
#include <uwebsockets/App.h>

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
            NoSSLApp = std::make_unique<uWS::App>();
        }
    }

    // WebSocket setup with ssl
    template<typename TPerSocketData>
    FSocketAppWrapper& wsssl(const std::string& pattern, uWS::TemplatedApp<true>::WebSocketBehavior<TPerSocketData> BehaviorWithSSL)
    {
		SSLApp->ws<TPerSocketData>(pattern, std::forward<uWS::TemplatedApp<true>::WebSocketBehavior<TPerSocketData>>(BehaviorWithSSL));

		return *this;
    }

    // WebSocket setup WITHOUT ssl
    template<typename TPerSocketData>
    FSocketAppWrapper& wssslno(const std::string& pattern, uWS::TemplatedApp<false>::WebSocketBehavior<TPerSocketData> BehaviourNoSSL)
    {
        NoSSLApp->ws<TPerSocketData>(pattern, std::forward<uWS::TemplatedApp<false>::WebSocketBehavior<TPerSocketData>>(BehaviourNoSSL));

        return *this;
    }

    // Listen
    FSocketAppWrapper& listen(std::string host, int port, auto&& handler)
    {
        if (bUseSSL)
        {
            SSLApp->listen(host, port, std::forward<decltype(handler)>(handler));
        }
        else
        {
            NoSSLApp->listen(host, port, std::forward<decltype(handler)>(handler));
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

    uWS::Loop* GetLoop() const
    {
        return bUseSSL ? SSLApp->getLoop() : NoSSLApp->getLoop();
    }

    bool IsUsingSSL() const { return bUseSSL; }

private:
    std::unique_ptr<uWS::App> NoSSLApp;
    std::unique_ptr<uWS::SSLApp> SSLApp;
    bool bUseSSL;
};
