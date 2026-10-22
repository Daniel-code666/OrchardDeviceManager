#include "RecoveryAuthentication.h"

#include <mbedtls/md.h>

namespace
{
    constexpr char HEX[] = "0123456789abcdef";

    void AppendField(std::string &payload, const std::string &value)
    {
        payload += std::to_string(value.size());
        payload.push_back(':');
        payload += value;
        payload.push_back('\n');
    }

    int Nibble(char value)
    {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        return -1;
    }

    bool DecodeKey(const std::string &hex, std::string &key)
    {
        if (hex.size() != 64)
            return false;
        key.assign(32, '\0');
        for (size_t index = 0; index < key.size(); ++index)
        {
            const int high = Nibble(hex[index * 2]);
            const int low = Nibble(hex[index * 2 + 1]);
            if (high < 0 || low < 0)
                return false;
            key[index] = static_cast<char>((high << 4) | low);
        }
        return true;
    }

    bool ConstantTimeEquals(const std::string &left, const std::string &right)
    {
        if (left.size() != right.size())
            return false;
        unsigned char difference = 0;
        for (size_t index = 0; index < left.size(); ++index)
            difference |= static_cast<unsigned char>(left[index] ^ right[index]);
        return difference == 0;
    }
}

std::string RecoveryAuthentication::BuildCanonicalPayload(const RecoveryCommand &command)
{
    std::string payload;
    AppendField(payload, std::to_string(command.contractVersion));
    AppendField(payload, command.deviceId);
    AppendField(payload, command.requestId);
    AppendField(payload, std::to_string(command.commandVersion));
    AppendField(payload, ToString(command.action));
    return payload;
}

bool RecoveryAuthentication::Verify(const RecoveryCommand &command, const std::string &keyHex)
{
    if (command.authenticationTag.size() != 64)
        return false;

    std::string key;
    if (!DecodeKey(keyHex, key))
        return false;

    const mbedtls_md_info_t *algorithm = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (algorithm == nullptr)
        return false;

    const std::string payload = BuildCanonicalPayload(command);
    unsigned char digest[32]{};
    if (mbedtls_md_hmac(algorithm,
                        reinterpret_cast<const unsigned char *>(key.data()), key.size(),
                        reinterpret_cast<const unsigned char *>(payload.data()), payload.size(),
                        digest) != 0)
        return false;

    std::string expected(sizeof(digest) * 2, '0');
    for (size_t index = 0; index < sizeof(digest); ++index)
    {
        expected[index * 2] = HEX[digest[index] >> 4];
        expected[index * 2 + 1] = HEX[digest[index] & 0x0f];
    }
    return ConstantTimeEquals(expected, command.authenticationTag);
}

