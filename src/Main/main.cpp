#include <Arduino.h>
#include "../Sensors/Sensors.h"
#include "../Constants/Constants.h"
#include "../Comm/MqttCommunication.h"
#include "../Secrets.h"
#include "Objects/FullTelemetry.h"
#include "ActionDispatcher/ActionDispatcher.h"

// declaración global de instancias únicamente para la clase actual
namespace
{
    Sensors sensors = Sensors();
    ActionDispatcher dispatcher(sensors);
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

    ActionMessage actionMessage{};

    if (comm.TryTakeAction(actionMessage))
    {
        ActionResult result = dispatcher.Dispatch(actionMessage);

        Serial.printf("Comando ejecutado: %s\n",result.success ? "SI" : "NO");

        comm.PublishActionResponse(result);
    }

    if (currentTime - lastRead >= READ_INTERVAL)
    {
        lastRead = currentTime;

        FullTelemetry fullTelemetry = sensors.GetDataFromSensors(true);

        comm.PublishTelemetry(fullTelemetry);
    }
}