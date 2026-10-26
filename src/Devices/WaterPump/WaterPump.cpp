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
    available = true;
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
bool WaterPump::SetState(float moisturePercentage,float drySoilPercentage,float wetSoilPercentage,std::optional<bool> state)
{
    const bool currentState = digitalRead(devicePin) == HIGH;

    if (mode == PumpControlMode::Manual)
    {
        if (!state.has_value())
            return false;

        const bool requestedState = state.value();

        if (currentState != requestedState)
        {
            digitalWrite(devicePin, requestedState ? HIGH : LOW);

            Serial.println(
                requestedState
                    ? "BOMBA ENCENDIDA MANUALMENTE"
                    : "BOMBA APAGADA MANUALMENTE"
            );
        }

        return true;
    }

    if (mode == PumpControlMode::Automatic)
    {
        // Validar las lecturas y el orden de los umbrales
        if (!std::isfinite(moisturePercentage) ||
            !std::isfinite(drySoilPercentage) ||
            !std::isfinite(wetSoilPercentage) ||
            drySoilPercentage >= wetSoilPercentage)
        {
            Serial.println("ERROR: DATOS DE HUMEDAD INVALIDOS");
            return false;
        }

        if (moisturePercentage <= drySoilPercentage && !currentState)
        {
            digitalWrite(devicePin, HIGH);

            Serial.println("BOMBA ACTIVADA AUTOMATICAMENTE");
        }
        else if (moisturePercentage >= wetSoilPercentage && currentState)
        {
            digitalWrite(devicePin, LOW);

            Serial.println("BOMBA DESACTIVADA AUTOMATICAMENTE");
        }

        return true;
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
