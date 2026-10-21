#include "ActionDispatcher.h"

namespace
{
    EnumsSwitch enumSwitch = EnumsSwitch();
}

ActionDispatcher::ActionDispatcher(Sensors& sensors) : sensors(sensors)
{

}

ActionResult ActionDispatcher::Dispatch(ActionMessage message)
{
    ActionResult result;

    DeviceIdByType ids = enumSwitch.OrganizeIdsByType(message);

    switch(message.command)
    {
        case Commands::GetSensorOrActuatorDataById:
        {
            result.telemetry = sensors.GetSensorOrActuatorDataById(ids);
            result.success = !result.telemetry.sensorReadings.empty() || !result.telemetry.actuatorsData.empty();
            break;
        }
        case Commands::SetWaterPumpControlMode:
        {
            result.success = sensors.SetWaterPumpMode(true);
            break;
        }
        case Commands::SetWaterPumpState:
        {
            break;
        }
    }

    return result;
}

