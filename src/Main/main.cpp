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
    MqttCommunication comm = MqttCommunication(MQTT_HOST, MQTT_PORT, MQTT_CLIENT_ID);
    unsigned long lastRead = 0;
}

void WiFiConn()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.print("\nWiFi conectado | IP: \n");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("\nWiFi no conectado\n");
    }
}

void setup()
{
//     put your setup code here, to run once:
    Serial.begin(115200);

    unsigned long start = millis();

    while (!Serial && millis() - start < 3000)
    {
        delay(10);
    }

    sensors.InitializeSensors();
    comm.init();

    Serial.print("Sensores inicializados...\n");
    Serial.print("LECTURAS OBTENIDAS \n");

    sensors.GetDataFromSensors(true);
    lastRead = millis();
}

void loop()
{
    unsigned long currentTime = millis();

    if (currentTime - lastRead >= READ_INTERVAL)
    {
        lastRead = currentTime;

        WiFiConn();

        FullTelemetry fullTelemetry = sensors.GetDataFromSensors(true);
        sensors.SetWaterPumpMode(1);
        //sensors.SetWaterPumpState();
    }
}