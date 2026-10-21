#ifndef ACTION_RESULT_H
#define ACTION_RESULT_H

#include "Objects/FullTelemetry.h"

struct ActionResult
{
    bool success = false;
    FullTelemetry telemetry;
};

#endif