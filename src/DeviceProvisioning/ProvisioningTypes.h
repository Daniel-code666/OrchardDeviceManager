#pragma once

#include <cstdint>
#include <string>

enum class DeviceProvisioningState : uint8_t
{
    Unprovisioned = 0,
    PendingClaim = 1,
    Provisioned = 2
};

enum class ClaimDecision : uint8_t
{
    Approved,
    Rejected,
    Expired
};

enum class ClaimProcessingResult : uint8_t
{
    Approved,
    Rejected,
    Expired,
    InvalidState,
    InvalidContract,
    RequestMismatch,
    AuthenticationFailed,
    StorageError
};

struct ProvisioningRecord
{
    static constexpr uint16_t CURRENT_VERSION = 1;

    uint16_t version = CURRENT_VERSION;
    DeviceProvisioningState state = DeviceProvisioningState::Unprovisioned;
    std::string requestId;
    std::string claimCode;
    std::string userId;
    std::string gardenId;

    bool IsValid() const;
};

struct ClaimRequest
{
    static constexpr uint16_t CONTRACT_VERSION = 1;

    uint16_t contractVersion = CONTRACT_VERSION;
    std::string deviceId;
    std::string requestId;
    std::string claimCode;
    uint32_t expiresInSeconds = 0;
};

struct ClaimResponse
{
    static constexpr uint16_t CONTRACT_VERSION = 1;

    uint16_t contractVersion = CONTRACT_VERSION;
    std::string deviceId;
    std::string requestId;
    ClaimDecision decision = ClaimDecision::Rejected;
    std::string userId;
    std::string gardenId;
    std::string errorCode;
    std::string authenticationTag;
};

bool IsValidProvisioningIdentifier(const std::string &value, size_t maximumLength = 128);
const char *ToString(DeviceProvisioningState state);
const char *ToString(ClaimDecision decision);
const char *ToString(ClaimProcessingResult result);

