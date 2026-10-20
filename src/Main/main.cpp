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

bool WiFiConn()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.print("\nWiFi conectado | IP: \n");
        Serial.println(WiFi.localIP());
        return true;
    }
    else
    {
        Serial.println("\nWiFi no conectado\n");
        return false;
    }
}

void setup()
{
//     put your setup code here, to run once:
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
    unsigned long currentTime = millis();

    if (currentTime - lastRead >= READ_INTERVAL)
    {
        lastRead = currentTime;

        if (WiFiConn())
        {
            comm.Connect();
        }

        FullTelemetry fullTelemetry = sensors.GetDataFromSensors(true);
        sensors.SetWaterPumpMode(1);
        
        comm.PublishTelemetry(fullTelemetry);
        //sensors.SetWaterPumpState();
    }
}