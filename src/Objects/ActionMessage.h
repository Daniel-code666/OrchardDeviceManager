#ifndef ACTION_MESSAGE_H
#define ACTION_MESSAGE_H

#include "Commands.h"
#include "ParamsBody.h"
#include <string>
#include <vector>

struct ActionMessage
{
    Commands command;
    std::vector<ParamsBody> params;
};

#endif