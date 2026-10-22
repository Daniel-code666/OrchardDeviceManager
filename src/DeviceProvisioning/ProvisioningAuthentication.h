#pragma once

#include "ProvisioningTypes.h"

#include <string>

class ProvisioningAuthentication
{
public:
    static bool Verify(const ClaimResponse &response, const std::string &claimCode);
    static std::string BuildCanonicalPayload(const ClaimResponse &response);
};

