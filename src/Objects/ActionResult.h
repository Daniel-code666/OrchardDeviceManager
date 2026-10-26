#ifndef ACTION_RESULT_H
#define ACTION_RESULT_H

#include "Objects/FullTelemetry.h"
#include <string>
#include "Commands.h"

struct ActionResult
{
    std::string requestId;
    Commands command = Commands::GetSensorOrActuatorDataById;
    bool success = false;
    FullTelemetry telemetry;
};

#endif
