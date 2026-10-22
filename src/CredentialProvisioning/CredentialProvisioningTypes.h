#pragma once

#include "../Configuration/DeviceConfiguration.h"

#include <cstdint>
#include <string>

struct CredentialSecurityRecord
{
    static constexpr uint16_t CURRENT_VERSION = 1;

    uint16_t version = CURRENT_VERSION;
    std::string encryptionKeyHex;
    uint32_t appliedCredentialVersion = 0;
    std::string pendingRequestId;

    bool IsValid() const;
};

struct MqttCredentialRequest
{
    static constexpr uint16_t CONTRACT_VERSION = 1;

    uint16_t contractVersion = CONTRACT_VERSION;
    std::string deviceId;
    std::string requestId;
    uint32_t currentCredentialVersion = 0;
};

struct EncryptedMqttCredentials
{
    static constexpr uint16_t CONTRACT_VERSION = 1;

    uint16_t contractVersion = CONTRACT_VERSION;
    std::string deviceId;
    std::string requestId;
    std::string nonceHex;
    std::string ciphertextHex;
    std::string authenticationTagHex;
};

enum class CredentialEnvelopeResult : uint8_t
{
    Accepted,
    InvalidState,
    InvalidContract,
    RequestMismatch,
    AuthenticationFailed,
    StaleVersion,
    InvalidCredentials,
    StorageError
};

const char *ToString(CredentialEnvelopeResult result);

