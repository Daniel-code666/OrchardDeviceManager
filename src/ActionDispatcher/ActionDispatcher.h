#ifndef ACTION_DISPATCHER_H
#define ACTION_DISPATCHER_H

#include "Sensors/Sensors.h"
#include "Objects/ActionResult.h"
#include "Objects/ActionMessage.h"
#include "Utils/EnumsSwitch.h"

class ActionDispatcher
{
    public:
        explicit ActionDispatcher(Sensors& sensors);
        ActionResult Dispatch(ActionMessage message);
    private:
        Sensors& sensors;
};

#endif