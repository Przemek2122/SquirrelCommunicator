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
        for (auto& byte : Salt)
        {
            byte = static_cast<uint8_t>(rd() % 256);
        }

        std::vector<uint8_t> Hash(Settings.hash_len);

        int Result = argon2id_hash_raw(
            Settings.t_cost,
            Settings.m_cost,
            Settings.parallelism,
            InputString.c_str(), InputString.length(),
            Salt.data(), Salt.size(),
            Hash.data(), Hash.size()
        );

        if (Result != ARGON2_OK) return "";

        // Encode as $argon2id$v=19$m=COST,t=TIME,p=PAR$SALT$HASH
        std::ostringstream oss;
        oss << "$argon2id$v=" << ARGON2_VERSION_NUMBER
            << "$m=" << Settings.m_cost << ",t=" << Settings.t_cost << ",p=" << Settings.parallelism
            << "$";

        // Hex encode salt and hash
        for (auto b : Salt) oss << std::hex << std::setfill('0') << std::setw(2) << (int)b;
        oss << "$";
        for (auto b : Hash) oss << std::hex << std::setfill('0') << std::setw(2) << (int)b;

        return oss.str();
    }

    std::string HashPassword(const std::string& InputString) override
    {
        // Use argon2 encoded hash for easy verification
        char encoded[256];
        FArgonSettings settings;

        std::vector<uint8_t> salt(settings.salt_len);
        std::random_device rd;
        for (auto& byte : salt) byte = static_cast<uint8_t>(rd() % 256);

        int result = argon2id_hash_encoded(
            settings.t_cost,
            settings.m_cost,
            settings.parallelism,
            InputString.c_str(), InputString.length(),
            salt.data(), salt.size(),
            settings.hash_len,
            encoded, sizeof(encoded)
        );

        if (result != ARGON2_OK) return "";
        return std::string(encoded);
    }

    bool VerifyPassword(const std::string& StringWithHash, const std::string& StringWithoutHash) override
    {
        int result = argon2id_verify(
            StringWithHash.c_str(),
            StringWithoutHash.c_str(),
            StringWithoutHash.length()
        );
        return result == ARGON2_OK;
    }
};
