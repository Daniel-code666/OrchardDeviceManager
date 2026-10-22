#pragma once

#include "RecoveryTypes.h"

#include <string>

class RecoveryAuthentication
{
public:
    static bool Verify(const RecoveryCommand &command, const std::string &keyHex);
    static std::string BuildCanonicalPayload(const RecoveryCommand &command);
};

