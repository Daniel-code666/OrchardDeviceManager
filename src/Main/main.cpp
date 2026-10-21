#include <Arduino.h>
#include "../Sensors/Sensors.h"
#include "../Constants/Constants.h"
#include "../Comm/MqttCommunication.h"
#include "../Secrets.h"
#include "Objects/FullTelemetry.h"

// declaración global de instancias únicamente para la clase actual
namespace
{
    Sensors sensors = Sensors();
    MqttCommunication comm = MqttCommunication(MQTT_HOST, MQTT_PORT, MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD);
    unsigned long lastRead = 0;
}

void setup()
{
    //put your setup code here, to run once:
    Serial.begin(115200);

    sensors.InitializeSensors();
    comm.Init();

    Serial.print("Sensores inicializados...\n");
    Serial.print("LECTURAS OBTENIDAS \n");

    // sensors.GetDataFromSensors(true);
    lastRead = millis();
}

void loop()
{
    comm.Loop();

    unsigned long currentTime = millis();

    if (currentTime - lastRead >= READ_INTERVAL)
    {
        lastRead = currentTime;

        FullTelemetry fullTelemetry = sensors.GetDataFromSensors(true);

        comm.PublishTelemetry(fullTelemetry);
    }
}