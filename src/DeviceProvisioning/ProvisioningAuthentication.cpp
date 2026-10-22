#include "ProvisioningAuthentication.h"

#include <mbedtls/md.h>

namespace
{
    constexpr char HEX[] = "0123456789abcdef";

    std::string ToHex(const unsigned char *bytes, size_t length)
    {
        std::string result(length * 2, '0');
        for (size_t index = 0; index < length; ++index)
        {
            result[index * 2] = HEX[(bytes[index] >> 4) & 0x0f];
            result[index * 2 + 1] = HEX[bytes[index] & 0x0f];
        }
        return result;
    }

    void AppendField(std::string &payload, const std::string &value)
    {
        payload += std::to_string(value.size());
        payload.push_back(':');
        payload += value;
        payload.push_back('\n');
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

std::string ProvisioningAuthentication::BuildCanonicalPayload(const ClaimResponse &response)
{
    std::string payload;
    payload.reserve(256);
    AppendField(payload, std::to_string(response.contractVersion));
    AppendField(payload, response.deviceId);
    AppendField(payload, response.requestId);
    AppendField(payload, ToString(response.decision));
    AppendField(payload, response.userId);
    AppendField(payload, response.gardenId);
    AppendField(payload, response.errorCode);
    return payload;
}

bool ProvisioningAuthentication::Verify(const ClaimResponse &response, const std::string &claimCode)
{
    if (claimCode.empty() || response.authenticationTag.size() != 64)
        return false;

    const mbedtls_md_info_t *algorithm = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (algorithm == nullptr)
        return false;

    const std::string payload = BuildCanonicalPayload(response);
    unsigned char digest[32]{};
    if (mbedtls_md_hmac(algorithm,
                        reinterpret_cast<const unsigned char *>(claimCode.data()), claimCode.size(),
                        reinterpret_cast<const unsigned char *>(payload.data()), payload.size(), digest) != 0)
        return false;

    return ConstantTimeEquals(ToHex(digest, sizeof(digest)), response.authenticationTag);
}

