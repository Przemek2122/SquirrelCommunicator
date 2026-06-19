#pragma once

#include <string>
#include <string_view>

class FCookieHelper
{
public:
	static std::string GetCookieValue(std::string_view cookieHeader, std::string_view name) {
        std::string searchFor = std::string(name) + "=";
        size_t pos = cookieHeader.find(searchFor);

        if (pos == std::string_view::npos) {
            return "";
        }

        pos += searchFor.length();
        size_t endPos = cookieHeader.find(';', pos);

        if (endPos == std::string_view::npos) {
            return std::string(cookieHeader.substr(pos));
        }

        return std::string(cookieHeader.substr(pos, endPos - pos));
    }

};
