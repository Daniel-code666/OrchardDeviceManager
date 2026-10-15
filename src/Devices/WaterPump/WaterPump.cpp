#include "WaterPump.h"
#include "Constants/Constants.h"
#include <string>
#include <Arduino.h>

WaterPump::WaterPump(int id, int pin, std::string actuatorName) : Device(id, pin, actuatorName, ConnectionType::Digital)
{
}

void WaterPump::Init()
{
}

// devuelve el estado funcional de la bomba en términos de true/false 
bool WaterPump::GetState()
{
    int digitalRawValue = digitalRead(devicePin);

    if (digitalRawValue >= 1)
        return true;
    else if (digitalRawValue <= 0)
        return false;
    
    return false;
}

// establece el estado funcional de la bomba, manualSet sobrescribe el comportamiento automático activando/desactivando la bomba de manera inmediata
bool WaterPump::SetState(bool state, bool manualSet, bool autoSet, int drySoilValue, int wetSoilValue)
{

}

ActuatorData WaterPump::GetActuatorData()
{
    int digitalRawValue = digitalRead(devicePin);

    return {
        deviceId,
        deviceName,
        devicePin,
        connType,
        digitalRawValue
    };
}
