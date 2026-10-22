#pragma once

#include "OtaTypes.h"

#include <string>

class OtaAuthentication
{
public:
    static bool Verify(const FirmwareManifest &manifest);
    static std::string BuildCanonicalPayload(const FirmwareManifest &manifest);
};

