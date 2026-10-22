#pragma once

#include "RecoveryTypes.h"

#include <string>

class RecoveryJson
{
public:
    static bool DeserializeCommand(const std::string &json, RecoveryCommand &command);
};

