#include "MqttCredentialProvisioningManager.h"

#include "../DeviceProvisioning/ProvisioningTypes.h"

#include <ArduinoJson.h>
#include <esp_random.h>
#include <mbedtls/gcm.h>

namespace
{
    void AppendField(std::string &payload, const std::string &value)
    {
        payload += std::to_string(value.size());
        payload.push_back(':');
        payload += value;
        payload.push_back('\n');
    }

    bool HasRequiredCredentialLengths(const DeviceConfiguration &configuration)
    {
        return !configuration.mqttHost.empty() && configuration.mqttHost.size() <= 253 &&
               configuration.mqttPort != 0 &&
               !configuration.mqttClientId.empty() && configuration.mqttClientId.size() <= 128 &&
               !configuration.mqttUser.empty() && configuration.mqttUser.size() <= 256 &&
               !configuration.mqttPassword.empty() && configuration.mqttPassword.size() <= 512;
    }
}

MqttCredentialProvisioningManager::MqttCredentialProvisioningManager(ConfigurationManager &manager)
    : configurationManager(manager), candidateVersion(0), initialized(false), candidatePending(false)
{
}

bool MqttCredentialProvisioningManager::Begin(const std::string &stableDeviceId,
                                               const DeviceConfiguration &active,
                                               bool deviceIsProvisioned)
{
    if (!deviceIsProvisioned || !IsValidProvisioningIdentifier(stableDeviceId) ||
        active.deviceId != stableDeviceId || !active.IsPersistable() ||
        !active.HasWifiConfiguration())
        return false;

    deviceId = stableDeviceId;
    baseConfiguration = active;

    CredentialSecurityRecord loaded;
    const CredentialSecurityLoadResult loadResult = configurationManager.LoadCredentialSecurity(loaded);
    if (loadResult == CredentialSecurityLoadResult::Loaded)
    {
        securityRecord = std::move(loaded);
    }
    else if (loadResult == CredentialSecurityLoadResult::NotFound)
    {
        CredentialSecurityRecord created;
        created.encryptionKeyHex = GenerateRandomHex(32);
        if (!SaveSecurityRecord(created))
            return false;
    }
    else
    {
        return false;
    }

    initialized = true;
    candidatePending = false;
    if (!securityRecord.pendingRequestId.empty())
        return true;
    return securityRecord.appliedCredentialVersion == 0 ? StartRequest() : true;
}

bool MqttCredentialProvisioningManager::StartRequest()
{
    if (!initialized || candidatePending || !securityRecord.pendingRequestId.empty())
        return false;

    CredentialSecurityRecord candidate = securityRecord;
    candidate.pendingRequestId = GenerateRandomHex(16);
    return SaveSecurityRecord(candidate);
}

void MqttCredentialProvisioningManager::UpdateBaseConfiguration(
    const DeviceConfiguration &active)
{
    if (initialized && active.IsPersistable() && active.HasWifiConfiguration() &&
        active.deviceId == deviceId)
        baseConfiguration = active;
}

CredentialEnvelopeResult MqttCredentialProvisioningManager::ProcessEnvelope(
    const EncryptedMqttCredentials &envelope)
{
    if (!initialized || securityRecord.pendingRequestId.empty() || candidatePending)
        return CredentialEnvelopeResult::InvalidState;

    if (envelope.contractVersion != EncryptedMqttCredentials::CONTRACT_VERSION ||
        envelope.deviceId != deviceId || !IsValidProvisioningIdentifier(envelope.deviceId) ||
        envelope.requestId.size() != 32 || envelope.nonceHex.size() != 24 ||
        envelope.authenticationTagHex.size() != 32 || envelope.ciphertextHex.empty() ||
        envelope.ciphertextHex.size() > 4096 || envelope.ciphertextHex.size() % 2 != 0)
        return CredentialEnvelopeResult::InvalidContract;

    if (envelope.requestId != securityRecord.pendingRequestId)
        return CredentialEnvelopeResult::RequestMismatch;

    std::string plaintext;
    if (!Decrypt(envelope, securityRecord.encryptionKeyHex, plaintext))
        return CredentialEnvelopeResult::AuthenticationFailed;

    DeviceConfiguration candidate;
    uint32_t version = 0;
    if (!ParseCredentials(plaintext, candidate, version))
        return CredentialEnvelopeResult::InvalidCredentials;
    if (version <= securityRecord.appliedCredentialVersion)
        return CredentialEnvelopeResult::StaleVersion;

    candidateConfiguration = std::move(candidate);
    candidateVersion = version;
    candidatePending = true;
    return CredentialEnvelopeResult::Accepted;
}

bool MqttCredentialProvisioningManager::TryGetPendingRequest(MqttCredentialRequest &request) const
{
    if (!initialized || securityRecord.pendingRequestId.empty())
        return false;

    request.deviceId = deviceId;
    request.requestId = securityRecord.pendingRequestId;
    request.currentCredentialVersion = securityRecord.appliedCredentialVersion;
    return true;
}

bool MqttCredentialProvisioningManager::TryGetLocalBootstrapKey(std::string &keyHex) const
{
    if (!initialized)
        return false;
    keyHex = securityRecord.encryptionKeyHex;
    return true;
}

bool MqttCredentialProvisioningManager::TryTakeCandidate(DeviceConfiguration &configuration,
                                                          uint32_t &credentialVersion)
{
    if (!candidatePending)
        return false;
    configuration = candidateConfiguration;
    credentialVersion = candidateVersion;
    candidatePending = false;
    return true;
}

bool MqttCredentialProvisioningManager::ConfirmApplied(uint32_t credentialVersion)
{
    if (!initialized || credentialVersion <= securityRecord.appliedCredentialVersion ||
        securityRecord.pendingRequestId.empty())
        return false;

    CredentialSecurityRecord candidate = securityRecord;
    candidate.appliedCredentialVersion = credentialVersion;
    candidate.pendingRequestId.clear();
    return SaveSecurityRecord(candidate);
}

void MqttCredentialProvisioningManager::RejectCandidate()
{
    candidatePending = false;
    candidateVersion = 0;
    candidateConfiguration = DeviceConfiguration{};
}

void MqttCredentialProvisioningManager::Reset()
{
    securityRecord = CredentialSecurityRecord{};
    baseConfiguration = DeviceConfiguration{};
    candidateConfiguration = DeviceConfiguration{};
    deviceId.clear();
    candidateVersion = 0;
    initialized = false;
    candidatePending = false;
}

uint32_t MqttCredentialProvisioningManager::GetAppliedCredentialVersion() const
{
    return securityRecord.appliedCredentialVersion;
}

bool MqttCredentialProvisioningManager::SaveSecurityRecord(const CredentialSecurityRecord &candidate)
{
    if (!candidate.IsValid() || !configurationManager.SaveCredentialSecurity(candidate))
        return false;
    securityRecord = candidate;
    return true;
}

std::string MqttCredentialProvisioningManager::GenerateRandomHex(size_t byteCount)
{
    constexpr char HEX_ALPHABET[] = "0123456789abcdef";
    std::string value(byteCount * 2, '0');
    for (size_t index = 0; index < byteCount; ++index)
    {
        const uint8_t randomByte = static_cast<uint8_t>(esp_random());
        value[index * 2] = HEX_ALPHABET[randomByte >> 4];
        value[index * 2 + 1] = HEX_ALPHABET[randomByte & 0x0f];
    }
    return value;
}

std::string MqttCredentialProvisioningManager::BuildAdditionalAuthenticatedData(
    const EncryptedMqttCredentials &envelope)
{
    std::string data;
    AppendField(data, std::to_string(envelope.contractVersion));
    AppendField(data, envelope.deviceId);
    AppendField(data, envelope.requestId);
    return data;
}

bool MqttCredentialProvisioningManager::DecodeHex(const std::string &hex, std::string &bytes)
{
    if (hex.size() % 2 != 0)
        return false;
    bytes.assign(hex.size() / 2, '\0');
    const auto nibble = [](char value) -> int
    {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        return -1;
    };
    for (size_t index = 0; index < bytes.size(); ++index)
    {
        const int high = nibble(hex[index * 2]);
        const int low = nibble(hex[index * 2 + 1]);
        if (high < 0 || low < 0)
            return false;
        bytes[index] = static_cast<char>((high << 4) | low);
    }
    return true;
}

bool MqttCredentialProvisioningManager::Decrypt(const EncryptedMqttCredentials &envelope,
                                                 const std::string &keyHex,
                                                 std::string &plaintext)
{
    std::string key;
    std::string nonce;
    std::string ciphertext;
    std::string tag;
    if (!DecodeHex(keyHex, key) || key.size() != 32 ||
        !DecodeHex(envelope.nonceHex, nonce) || nonce.size() != 12 ||
        !DecodeHex(envelope.ciphertextHex, ciphertext) ||
        !DecodeHex(envelope.authenticationTagHex, tag) || tag.size() != 16)
        return false;

    const std::string aad = BuildAdditionalAuthenticatedData(envelope);
    plaintext.assign(ciphertext.size(), '\0');
    mbedtls_gcm_context context;
    mbedtls_gcm_init(&context);
    const int keyResult = mbedtls_gcm_setkey(
        &context, MBEDTLS_CIPHER_ID_AES,
        reinterpret_cast<const unsigned char *>(key.data()), 256);
    const int decryptResult = keyResult == 0
                                  ? mbedtls_gcm_auth_decrypt(
                                        &context, ciphertext.size(),
                                        reinterpret_cast<const unsigned char *>(nonce.data()), nonce.size(),
                                        reinterpret_cast<const unsigned char *>(aad.data()), aad.size(),
                                        reinterpret_cast<const unsigned char *>(tag.data()), tag.size(),
                                        reinterpret_cast<const unsigned char *>(ciphertext.data()),
                                        reinterpret_cast<unsigned char *>(plaintext.data()))
                                  : keyResult;
    mbedtls_gcm_free(&context);
    if (decryptResult != 0)
    {
        plaintext.clear();
        return false;
    }
    return true;
}

bool MqttCredentialProvisioningManager::ParseCredentials(const std::string &plaintext,
                                                          DeviceConfiguration &configuration,
                                                          uint32_t &credentialVersion) const
{
    JsonDocument document;
    if (deserializeJson(document, plaintext) != DeserializationError::Ok)
        return false;

    const char *host = document["host"];
    const char *clientId = document["clientId"];
    const char *user = document["user"];
    const char *password = document["password"];
    const uint32_t version = document["credentialVersion"] | 0;
    const uint32_t port = document["port"] | 0;
    if (host == nullptr || clientId == nullptr || user == nullptr || password == nullptr ||
        version == 0 || port == 0 || port > 65535)
        return false;

    if (baseConfiguration.mqttClientId != clientId)
        return false;

    DeviceConfiguration parsed = baseConfiguration;
    parsed.mqttHost = host;
    parsed.mqttPort = static_cast<uint16_t>(port);
    parsed.mqttUser = user;
    parsed.mqttPassword = password;
    if (!HasRequiredCredentialLengths(parsed) || !parsed.IsValid())
        return false;

    configuration = std::move(parsed);
    credentialVersion = version;
    return true;
}

