#include "ManagedProvisioningTypes.h"

#include "../DeviceProvisioning/ProvisioningTypes.h"

namespace
{
    bool HasPrefix(const std::string &value, const char *prefix)
    {
        return value.rfind(prefix, 0) == 0;
    }

    bool IsValidServiceUrl(const std::string &value)
    {
        if (value.size() < 12 || value.size() > 256 ||
            value.find_first_of("\r\n\t ") != std::string::npos ||
            value.find('#') != std::string::npos || value.find('?') != std::string::npos)
            return false;

        const bool https = HasPrefix(value, "https://");
#if ENABLE_LOCAL_PROVISIONING_MOCK
        const bool developmentHttp = HasPrefix(value, "http://");
#else
        const bool developmentHttp = false;
#endif
        return https || developmentHttp;
    }
}

bool ManagedBootstrapRequest::IsValid() const
{
    return contractVersion == CONTRACT_VERSION &&
           IsValidProvisioningIdentifier(requestId, 64) &&
           !wifiSsid.empty() && wifiSsid.size() <= 32 &&
           wifiPassword.size() <= 64 &&
           (wifiPassword.empty() || wifiPassword.size() >= 8) &&
           IsValidServiceUrl(serviceBaseUrl) &&
           provisioningToken.size() >= 16 && provisioningToken.size() <= 512;
}

bool ManagedProvisioningRecord::IsValid() const
{
    ManagedBootstrapRequest request;
    request.requestId = bootstrapRequestId;
    request.wifiSsid = "persisted";
    request.serviceBaseUrl = serviceBaseUrl;
    request.provisioningToken = provisioningToken;
    return version == CURRENT_VERSION && request.IsValid();
}

