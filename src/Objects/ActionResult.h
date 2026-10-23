#ifndef ACTION_RESULT_H
#define ACTION_RESULT_H

#include "Objects/FullTelemetry.h"
#include <string>

struct ActionResult
{
    std::string requestId;
    bool success = false;
    FullTelemetry telemetry;
};

#endif