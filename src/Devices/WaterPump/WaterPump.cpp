#include "WaterPump.h"
#include "Constants/Constants.h"
#include <string>
#include <Arduino.h>

WaterPump::WaterPump(int id, int pin, std::string actuatorName) : Device(id, pin, actuatorName, ConnectionType::Digital)
{
}

// inicializa la bomba con el estado apagado
void WaterPump::Init()
{
    digitalWrite(devicePin, false);
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
bool WaterPump::SetState(bool state, bool manualSet, bool autoSet, float moisturePercentage, float drySoilPercentage, float wetSoilPercentage)
{
    int digitalRawValue = digitalRead(devicePin);

    if (manualSet)
    {
        if (digitalRawValue != state)
        {
            digitalWrite(devicePin, state);

            Serial.print("ESTADO DE LA BOMBA ESTABLECIDO");
            Serial.print("\nESTADO DE LA BOMBA\n");
            Serial.print(digitalRawValue);
            Serial.print("\nESTADO ENVIADO\n");
            Serial.print(state);
            Serial.print("\n");

            return true;
        }
        else
        {
            Serial.print("BOMBA CON EL ESTADO ACTUAL QUE SE INTENTA ESTABLECER\n");
            Serial.print("\nESTADO DE LA BOMBA\n");
            Serial.print(digitalRawValue);
            Serial.print("\nESTADO ENVIADO\n");
            Serial.print(state);
            Serial.print("\n");

            return false;
        }
    }
    else if (autoSet && !manualSet) 
    {
        if (moisturePercentage <= drySoilPercentage)
        {
            Serial.print("BOMBA DE AGUA ACTIVA, REGANDO PLANTAS");
            digitalWrite(devicePin, true);

            return true;
        }
        else if (moisturePercentage >= wetSoilPercentage)
        {
            Serial.print("BOMBA DE AGUA INACTIVA");
            digitalWrite(devicePin, false);

            return true;
        }

        return false;
    }

    return false;
}

ActuatorData WaterPump::GetActuatorData()
{
    int digitalRawValue = digitalRead(devicePin);

    return {
        deviceId,
        deviceName,
        devicePin,
        connType,
        static_cast<float>(digitalRawValue)
    };
}
