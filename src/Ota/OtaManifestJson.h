#pragma once

#include "OtaTypes.h"

#include <string>

class OtaManifestJson
{
public:
    static bool Deserialize(const std::string &json, FirmwareManifest &manifest);
};

