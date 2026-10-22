#pragma once

#include "ProvisioningTypes.h"

#include <string>

class ProvisioningJson
{
public:
    static std::string Serialize(const ClaimRequest &request);
    static bool DeserializeResponse(const std::string &json, ClaimResponse &response);
};

