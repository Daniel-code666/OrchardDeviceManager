#pragma once

#include "OtaTypes.h"
#include <string>

class OtaCommandAuthentication
{
public:
    static std::string BuildCanonicalPayload(const RemoteOtaRequest &request);
    static bool Verify(const RemoteOtaRequest &request, const std::string &keyHex);
};

