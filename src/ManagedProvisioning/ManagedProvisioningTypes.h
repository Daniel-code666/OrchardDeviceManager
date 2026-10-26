#pragma once

#include <cstdint>
#include <string>

struct ManagedBootstrapRequest
{
    static constexpr uint16_t CONTRACT_VERSION = 1;

    uint16_t contractVersion = CONTRACT_VERSION;
    std::string requestId;
    std::string wifiSsid;
    std::string wifiPassword;
    std::string serviceBaseUrl;
    std::string provisioningToken;

    bool IsValid() const;
};

struct ManagedProvisioningRecord
{
    static constexpr uint16_t CURRENT_VERSION = 1;

    uint16_t version = CURRENT_VERSION;
    std::string bootstrapRequestId;
    std::string serviceBaseUrl;
    std::string provisioningToken;

    bool IsValid() const;
};

