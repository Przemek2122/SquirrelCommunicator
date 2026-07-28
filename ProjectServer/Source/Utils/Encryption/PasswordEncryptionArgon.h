// Password Encryption using Argon2
// Standalone replacement for Engine's FPasswordEncryptionArgon

#pragma once

#include <string>
#include <cstdint>
#include <argon2.h>
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>

struct FArgonSettings
{
    FArgonSettings(uint32_t t_cost = 2, uint32_t m_cost = 16 * 1024, uint32_t parallelism = 1, uint32_t hash_len = 32, uint32_t salt_len = 16)
        : t_cost(t_cost)
        , m_cost(m_cost)
        , parallelism(parallelism)
        , hash_len(hash_len)
        , salt_len(salt_len)
    {
    }

    uint32_t t_cost;
    uint32_t m_cost;
    uint32_t parallelism;
    uint32_t hash_len;
    uint32_t salt_len;
};

class FPasswordEncryptionBase
{
public:
    virtual ~FPasswordEncryptionBase() = default;
    virtual std::string HashPassword(const std::string& InputString) = 0;
    virtual bool VerifyPassword(const std::string& StringWithHash, const std::string& StringWithoutHash) = 0;
};

class FPasswordEncryptionArgon : public FPasswordEncryptionBase
{
public:
    std::string HashPasswordCustom(const std::string& InputString, const FArgonSettings& Settings)
    {
        // Generate random salt
        std::vector<uint8_t> Salt(Settings.salt_len);
        std::random_device rd;
        for (uint8_t& byte : Salt)
        {
            byte = static_cast<uint8_t>(rd() % 256);
        }

        // Calculate correct encoded buffer length
        const size_t encoded_len = argon2_encodedlen(
            Settings.t_cost,
            Settings.m_cost,
            Settings.parallelism,
            static_cast<uint32_t>(Salt.size()),
            Settings.hash_len,
            Argon2_id
        );

        std::vector<char> Encoded(encoded_len);

        // Hash and let Argon2 format the B64 string properly
        const int Result = argon2id_hash_encoded(
            Settings.t_cost,
            Settings.m_cost,
            Settings.parallelism,
            InputString.c_str(), InputString.length(),
            Salt.data(), Salt.size(),
            Settings.hash_len,
            Encoded.data(), Encoded.size()
        );

        if (Result != ARGON2_OK) return "";

        return std::string(Encoded.data());
    }

    std::string HashPassword(const std::string& InputString) override
    {
        FArgonSettings settings;

        // Generate salt
        std::vector<uint8_t> salt(settings.salt_len);
        std::random_device rd;
        for (uint8_t& byte : salt) {
            byte = static_cast<uint8_t>(rd() % 256);
        }

        // Compute buffer length with argon2_encodedlen
        const size_t encoded_len = argon2_encodedlen(
            settings.t_cost,
            settings.m_cost,
            settings.parallelism,
            static_cast<uint32_t>(salt.size()),
            settings.hash_len,
            Argon2_id // Pass the type explicitly
        );

        // Allocate buffer
        std::vector<char> encoded(encoded_len);

        // Hash encoded
        const int result = argon2id_hash_encoded(
            settings.t_cost,
            settings.m_cost,
            settings.parallelism,
            InputString.c_str(), InputString.length(),
            salt.data(), salt.size(),
            settings.hash_len,
            encoded.data(), encoded.size()
        );

        if (result != ARGON2_OK) {
            // Option to inspect error: argon2_error_message(result)
            return "";
        }

        return std::string(encoded.data());
    }

    bool VerifyPassword(const std::string& StringWithHash, const std::string& StringWithoutHash) override
    {
        const int result = argon2id_verify(
            StringWithHash.c_str(),
            StringWithoutHash.c_str(),
            StringWithoutHash.length()
        );

        return result == ARGON2_OK;
    }
};
