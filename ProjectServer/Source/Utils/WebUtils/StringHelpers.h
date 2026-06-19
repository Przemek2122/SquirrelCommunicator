// Created by https://www.linkedin.com/in/przemek2122/ 2024

#pragma once

#include <string>
#include <string_view>
#include <vector>

class FStringHelpers
{
public:
	static bool CompareStringCaseInsensitive(const std::string& A, const std::string& B);
	static bool CompareCharsCaseInsensitive(char A, char B);

	static bool ContainsChar(const std::string& String, const char SearchFor);

	static bool ToBoolValue(const std::string& String);

	static std::string ReplaceCharInString(const std::string& BaseString, const char ReplaceFrom, const char ReplaceTo);
	static std::string ReplaceCharsInString(const std::string& BaseString, const std::vector<char>& ReplaceFrom, const char ReplaceTo);

	static std::string RemoveCharInString(const std::string& BaseString, const char RemovedChar);
	static std::string RemoveCharsInString(const std::string& BaseString, const std::vector<char>& ReplaceFrom);

	static std::vector<std::string> SplitString(const std::string& BaseString, const char Delimiter);

    struct FStringValidationResult
	{
        bool bIsValid;
        std::vector<char> InvalidChars;
        std::string Message;

		explicit operator bool() const
		{
			return bIsValid;
		}
    };

	/** Check if given string consists only of given characters */
	static FStringValidationResult ValidateString(std::string_view InString, std::string_view AllowedCharSet);

	/** Check if given mail has @, i, etc... */
	static bool ValidateMail(const std::string& InEMail);

};
