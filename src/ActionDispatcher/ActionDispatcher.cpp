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

    switch(message.command)
    {
        case Commands::GetSensorOrActuatorDataById:
        {
            DeviceIdByType ids = enumSwitch.OrganizeIdsByType(message);
            result.telemetry = sensors.GetSensorOrActuatorDataById(ids);
            result.success = !result.telemetry.sensorReadings.empty() || !result.telemetry.actuatorsData.empty();
            break;
        }
        case Commands::SetWaterPumpControlMode:
        {
            std::vector<WaterPumpSetMode> resultModeAction = enumSwitch.SetDataForWaterPumpControlModeAction(message);
            result.success = sensors.SetWaterPumpMode(resultModeAction);
            break;
        }
        case Commands::SetWaterPumpState:
        {
            break;
        }
    }

    return result;
}

