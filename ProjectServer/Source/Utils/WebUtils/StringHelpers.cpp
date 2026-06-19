// Created by https://www.linkedin.com/in/przemek2122/ 2024

#include "StringHelpers.h"
#include "SQRLLEncryption.h"

bool FStringHelpers::CompareStringCaseInsensitive(const std::string& A, const std::string& B)
{
    return A.size() == B.size() && std::equal(A.begin(), A.end(), B.begin(), CompareCharsCaseInsensitive);
}

bool FStringHelpers::CompareCharsCaseInsensitive(const char A, const char B)
{
    return ( std::tolower(static_cast<unsigned char>(A)) == std::tolower(static_cast<unsigned char>(B)) );
}

bool FStringHelpers::ContainsChar(const std::string& String, const char SearchFor)
{
	bool bContainsChar = false;

    for (const char& Char : String)
    {
        if (Char == SearchFor)
        {
			bContainsChar = true;

			break;
        }
    }

    return bContainsChar;
}

bool FStringHelpers::ToBoolValue(const std::string& String)
{
    static std::string BoolTrueStringValue = "true";

    bool bIsTrue = false;

    if (CompareStringCaseInsensitive(String, BoolTrueStringValue))
    {
        bIsTrue = true;
    }

    return bIsTrue;
}

std::string FStringHelpers::ReplaceCharInString(const std::string& BaseString, const char ReplaceFrom, const char ReplaceTo)
{
    std::string OutputString;

    for (const char& Char : BaseString)
    {
	    if (Char == ReplaceFrom)
	    {
            OutputString += ReplaceTo;
	    }
        else
        {
            OutputString += Char;
        }
    }

    return OutputString;
}

std::string FStringHelpers::ReplaceCharsInString(const std::string& BaseString, const std::vector<char>& ReplaceFrom, const char ReplaceTo)
{
    std::string Out = BaseString;

    for (const char& FromChar : ReplaceFrom)
    {
        Out = ReplaceCharInString(Out, FromChar, ReplaceTo);
    }

    return Out;
}

std::string FStringHelpers::RemoveCharInString(const std::string& BaseString, const char RemovedChar)
{
    std::string OutputString;

    for (const char& Char : BaseString)
    {
        if (Char != RemovedChar)
        {
            OutputString += Char;
        }
    }

    return OutputString;
}

std::string FStringHelpers::RemoveCharsInString(const std::string& BaseString, const std::vector<char>& ReplaceFrom)
{
    std::string Out = BaseString;

    for (const char& FromChar : ReplaceFrom)
    {
        Out = RemoveCharInString(Out, FromChar);
    }

    return Out;
}

std::vector<std::string> FStringHelpers::SplitString(const std::string& BaseString, const char Delimiter)
{
	std::vector<std::string> OutArray;
	std::string CurrentString;

	for (const char& Char : BaseString)
	{
		if (Char == Delimiter)
		{
			OutArray.push_back(CurrentString);
			CurrentString.clear();
		}
		else
		{
			CurrentString += Char;
		}
	}

	if (!CurrentString.empty())
	{
		OutArray.push_back(CurrentString);
	}

	return OutArray;
}

FStringHelpers::FStringValidationResult FStringHelpers::ValidateString(std::string_view InString, std::string_view AllowedCharSet)
{
	FStringValidationResult result;
	result.bIsValid = true;

	for (char c : InString) {
		if (AllowedCharSet.find(c) == std::string_view::npos) {
			result.bIsValid = false;
			result.InvalidChars.push_back(c);
		}
	}

	if (!result.bIsValid) {
		result.Message = "Password contains invalid characters: ";
		for (size_t i = 0; i < result.InvalidChars.size(); ++i) {
			result.Message += "'";
			result.Message += result.InvalidChars[i];
			result.Message += "'";
			if (i < result.InvalidChars.size() - 1) {
				result.Message += ", ";
			}
		}
	}
	else {
		result.Message = "Password is valid";
	}

	return result;
}

bool FStringHelpers::ValidateMail(const std::string& InEMail)
{
	// Find @ sign
	const size_t AtPos = InEMail.find('@');
	if (AtPos == std::string::npos || AtPos == 0 || AtPos == InEMail.length() - 1)
	{
		return false;
	}

	// Check for multiple @
	if (InEMail.find('@', AtPos + 1) != std::string::npos)
	{
		return false;
	}

	// Find dot in domain part (after @)
	const size_t DotPos = InEMail.find('.', AtPos + 1);
	if (DotPos == std::string::npos || DotPos == AtPos + 1 || DotPos == InEMail.length() - 1)
	{
		return false;
	}

	if (!ValidateString(InEMail, SQRLLPredefinedCharsets::BASE_EMAIL))
	{
		return false;
	}

	return true;
}
