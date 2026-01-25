// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once
#include "nlohmann/json_fwd.hpp"

/** Class for sending e-mails - Currently supports brevo */
class FMailSender
{
public:
    /**
     * Utility for Email sending.
     * Sends given body to Brevo
     * Sample bodies:

            const nlohmann::json JsonBody = {
                {"to", {{{"email", TargetEmail}}}},
                {"templateId", 1},  // your template ID
                {"params", {
                    {"TOKEN", "76931F"}
                }}
            };

            const nlohmann::json JsonBody = {
                {"sender", {{"email", "no-reply@sqrll.net"}}},
                {"to", {{{"email", TargetEmail}}}},
                {"subject", Subject},
                {"htmlContent", HtmlContent}
            };
     */
    static void SendMail(const nlohmann::json& JsonBody);

};