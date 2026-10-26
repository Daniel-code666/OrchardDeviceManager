#include "OtaCommandAuthentication.h"

#include <mbedtls/md.h>

namespace
{
    constexpr char HEX[] = "0123456789abcdef";
    void Append(std::string &payload, const std::string &value)
    {
        payload += std::to_string(value.size()) + ":" + value + "\n";
    }
    int Nibble(char value)
    {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        return -1;
    }
    bool DecodeKey(const std::string &hex, std::string &key)
    {
        if (hex.size() != 64) return false;
        key.assign(32, '\0');
        for (size_t index = 0; index < key.size(); ++index)
        {
            const int high = Nibble(hex[index * 2]);
            const int low = Nibble(hex[index * 2 + 1]);
            if (high < 0 || low < 0) return false;
            key[index] = static_cast<char>((high << 4) | low);
        }
        return true;
    }
    bool ConstantTimeEquals(const std::string &left, const std::string &right)
    {
        if (left.size() != right.size()) return false;
        unsigned char difference = 0;
        for (size_t index = 0; index < left.size(); ++index)
            difference |= static_cast<unsigned char>(left[index] ^ right[index]);
        return difference == 0;
    }
}

std::string OtaCommandAuthentication::BuildCanonicalPayload(const RemoteOtaRequest &request)
{
    std::string payload;
    Append(payload, std::to_string(request.contractVersion));
    Append(payload, request.deviceId);
    Append(payload, request.requestId);
    Append(payload, std::to_string(request.commandVersion));
    Append(payload, request.releaseId);
    Append(payload, request.manifestUrl);
    Append(payload, request.expiresAt);
    return payload;
}

bool OtaCommandAuthentication::Verify(const RemoteOtaRequest &request,
                                      const std::string &keyHex)
{
    if (request.authenticationTag.size() != 64) return false;
    std::string key;
    if (!DecodeKey(keyHex, key)) return false;
    const mbedtls_md_info_t *algorithm = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (algorithm == nullptr) return false;
    const std::string payload = BuildCanonicalPayload(request);
    unsigned char digest[32]{};
    if (mbedtls_md_hmac(algorithm,
                        reinterpret_cast<const unsigned char *>(key.data()), key.size(),
                        reinterpret_cast<const unsigned char *>(payload.data()), payload.size(),
                        digest) != 0)
        return false;
    std::string expected(64, '0');
    for (size_t index = 0; index < sizeof(digest); ++index)
    {
        expected[index * 2] = HEX[digest[index] >> 4];
        expected[index * 2 + 1] = HEX[digest[index] & 0x0f];
    }
    return ConstantTimeEquals(expected, request.authenticationTag);
}
