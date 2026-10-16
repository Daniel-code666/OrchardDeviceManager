#include "WaterPump.h"
#include "Constants/Constants.h"
#include <string>
#include <Arduino.h>
#include <Utils/EnumsSwitch.h>

namespace
{
    EnumsSwitch enumsSwitch = EnumsSwitch();
}

WaterPump::WaterPump(int id, int pin, std::string actuatorName) : Device(id, pin, actuatorName, ConnectionType::Digital)
{
}

// inicializa la bomba con el estado apagado
void WaterPump::Init()
{
    digitalWrite(devicePin, LOW);
    pinMode(devicePin, OUTPUT);
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

// establece el estado funcional de la bomba, state puede ser null
bool WaterPump::SetState(float moisturePercentage, float drySoilPercentage, float wetSoilPercentage, std::optional<bool> state)
{
    int digitalRawValue = digitalRead(devicePin);

    if (mode == PumpControlMode::Manual && state.has_value())
    {
        if (digitalRawValue == state.value())
            return false;

        digitalWrite(devicePin, state.value() ? HIGH : LOW);

        Serial.println("ESTADO MANUAL DE LA BOMBA MODIFICADO");

        return true;
    }
    else if (mode == PumpControlMode::Automatic) 
    {
        if (moisturePercentage <= drySoilPercentage && !digitalRawValue)
        {
            Serial.print("BOMBA DE AGUA ACTIVA, REGANDO PLANTAS");
            digitalWrite(devicePin, true);

            return true;
        }
        else if (moisturePercentage >= wetSoilPercentage && digitalRawValue)
        {
            Serial.print("BOMBA DE AGUA INACTIVA");
            digitalWrite(devicePin, false);

            return true;
        }

        return false;
    }

    return false;
}

// obtiene la información básica del actuador
ActuatorData WaterPump::GetActuatorData()
{
    int digitalRawValue = digitalRead(devicePin);

    return {
        deviceId,
        deviceName,
        devicePin,
        connType,
        mode,
        static_cast<float>(digitalRawValue)
    };
}

// obtiene el modo de control de la bomba
std::string WaterPump::GetControlMode()
{
    return enumsSwitch.PumpControlModeName(mode);
}

// establece el modo de control de la bomba 1: manual 2: automático
bool WaterPump::SetMode(int opt)
{
    switch (opt)
    {
        case 1:
            mode = PumpControlMode::Manual;
            return true;
        case 2:
            mode = PumpControlMode::Automatic;
            return true;
        default:
            return false;
    }
}
