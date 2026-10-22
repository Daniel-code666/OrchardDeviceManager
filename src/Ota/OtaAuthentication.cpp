#include "OtaAuthentication.h"

#include "OtaPublicKey.h"

#include <cstring>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>

namespace
{
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

    bool DecodeHex(const std::string &hex, std::string &bytes)
    {
        if (hex.empty() || hex.size() % 2 != 0)
            return false;
        bytes.assign(hex.size() / 2, '\0');
        for (size_t index = 0; index < bytes.size(); ++index)
        {
            const int high = Nibble(hex[index * 2]);
            const int low = Nibble(hex[index * 2 + 1]);
            if (high < 0 || low < 0)
                return false;
            bytes[index] = static_cast<char>((high << 4) | low);
        }
        return true;
    }
}

std::string OtaAuthentication::BuildCanonicalPayload(const FirmwareManifest &manifest)
{
    std::string payload;
    payload.reserve(512);
    AppendField(payload, std::to_string(manifest.contractVersion));
    AppendField(payload, manifest.releaseId);
    AppendField(payload, manifest.version);
    AppendField(payload, manifest.board);
    AppendField(payload, std::to_string(manifest.size));
    AppendField(payload, manifest.sha256);
    AppendField(payload, manifest.firmwareUrl);
    return payload;
}

bool OtaAuthentication::Verify(const FirmwareManifest &manifest)
{
    std::string signature;
    if (!DecodeHex(manifest.signatureHex, signature) || signature.size() > 80)
        return false;

    const std::string payload = BuildCanonicalPayload(manifest);
    unsigned char digest[32]{};
    if (mbedtls_sha256(reinterpret_cast<const unsigned char *>(payload.data()), payload.size(),
                       digest, 0) != 0)
        return false;

    mbedtls_pk_context publicKey;
    mbedtls_pk_init(&publicKey);
    const int parseResult = mbedtls_pk_parse_public_key(
        &publicKey, reinterpret_cast<const unsigned char *>(OTA_SIGNING_PUBLIC_KEY),
        std::strlen(OTA_SIGNING_PUBLIC_KEY) + 1);
    const int verifyResult = parseResult == 0
                                 ? mbedtls_pk_verify(
                                       &publicKey, MBEDTLS_MD_SHA256, digest, sizeof(digest),
                                       reinterpret_cast<const unsigned char *>(signature.data()),
                                       signature.size())
                                 : parseResult;
    mbedtls_pk_free(&publicKey);
    return verifyResult == 0;
}

