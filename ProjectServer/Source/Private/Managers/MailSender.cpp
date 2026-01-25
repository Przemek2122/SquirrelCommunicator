// Created by https://www.linkedin.com/in/przemek2122/ 2026

#include "Managers/MailSender.h"
#include "ProjectEngine.h"

#include <cpr/api.h>
#include <cpr/body.h>

#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"

void FMailSender::SendMail(const nlohmann::json& JsonBody)
{
    FProjectEngine* ProjectEngine = dynamic_cast<FProjectEngine*>(FGlobalDefines::GEngine);

    std::string APIKey;
    if (ProjectEngine != nullptr)
    {
        APIKey = ProjectEngine->GetMailAPIKey();
    }

    if (!APIKey.empty())
    {
        try
        {
            const std::string JsonBodyString = JsonBody.dump();

            const cpr::Response Response = cpr::Post(
                cpr::Url{"https://api.brevo.com/v3/smtp/email"},
                cpr::Header{
                    {"api-key", APIKey},
                    {"accept", "application/json"},
                    {"content-type", "application/json"}
                },
                cpr::Body{JsonBodyString}
            );

            if (Response.status_code != 201)
            {
                throw std::runtime_error("HTTP " + std::to_string(Response.status_code) + ": " + Response.text);
            }
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error("Failed to send email: " + std::string(e.what()));
        }
    }
    else
    {
        LOG_ERROR("Mail will not be sent. Missing key!");
    }
}
