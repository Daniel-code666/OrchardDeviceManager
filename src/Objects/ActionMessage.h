#ifndef ACTION_MESSAGE_H
#define ACTION_MESSAGE_H

#include "Commands.h"
#include "ParamsBody.h"
#include <string>
#include <vector>
#include <cstdint>

struct ActionMessage
{
    uint16_t contractVersion = 1;
    std::string targetDeviceId;
    std::string requestId;
    Commands command;
    std::vector<ParamsBody> params;
};

#endif
