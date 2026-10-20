#include "MqttCommunication.h"
#include "Objects/Topics.h"
#include "../Utils/EnumsSwitch.h"
#include <ArduinoJson.h>
#include <string.h>

namespace
{
    EnumsSwitch enumSwitch = EnumsSwitch();
}

// constructor
MqttCommunication::MqttCommunication(const char *brokerHost, uint16_t brokerPort, const char *clientId,
                                     const char *brokerUser, const char *brokerPass) : wifiClient(), mqttClient(wifiClient), brokerHost(brokerHost), brokerPort(brokerPort), clientId(clientId),
                                                                                       brokerUser(brokerUser), brokerPass(brokerPass) {};

// valida la conexión wifi y conexión al broker
bool MqttCommunication::CheckWifiBrokerConnection()
{
    bool wifiStatus = true;
    bool brokerStatus = true;

    // validar conexión a wifi
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.print("\nConexión a mqtt, validación de wifi: no conectado\n");
        wifiStatus = false;
    }

    // validar conexión al broker
    if (!mqttClient.connected())
    {
        Serial.print("\nConexión a mqtt, validación de conexión a broker: no conectado\n");
        brokerStatus  = false;
    }

    return wifiStatus && brokerStatus;
}

// crea el json para luego parcearlo
JsonDocument LoadDataIntoJson(FullTelemetry &fullTelemetry)
{
    JsonDocument doc;

    JsonArray sensors = doc["sensorReadings"].to<JsonArray>();
    JsonArray actuators = doc["actuatorReadings"].to<JsonArray>();

    for (const SensorReading &reading : fullTelemetry.sensorReadings)
    {
        JsonObject sensor = sensors.add<JsonObject>();

        sensor["id"] = reading.sensorId;
        sensor["name"] = reading.sensorName;
        sensor["measurementType"] = enumSwitch.MeasurementTypesName(reading.measurementInfo.type);
        sensor["measurementUnit"] = enumSwitch.MeasurementUnitsName(reading.measurementInfo.unit);
        sensor["esp32Pin"] = reading.sensorPin;
        sensor["connType"] = enumSwitch.ConnectionTypeName(reading.connType);
        sensor["value"] = reading.value;
    }

    for (const ActuatorData &data : fullTelemetry.actuatorsData)
    {
        JsonObject actuator = actuators.add<JsonObject>();

        actuator["id"] = data.actuatorId;
        actuator["name"] = data.actuatorName;
        actuator["esp32Pin"] = data.actuatorPin;
        actuator["connType"] = enumSwitch.ConnectionTypeName(data.connType);
        actuator["mode"] = enumSwitch.PumpControlModeName(data.mode);
        actuator["state"] = data.value;
    }

    return doc;
}

// validación al momento de cualquier evento de wifi
void OnWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
{
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
    {
        Serial.print("Desconexion WiFi | Razon: ");
        Serial.println(info.wifi_sta_disconnected.reason);
    }
    else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP)
    {
        Serial.print("Conectado | IP: ");
        Serial.println(WiFi.localIP());
    }
}

// incializa el wifi y conecta con rabbitmq
void MqttCommunication::Init()
{
    // Establece el modo de wifi del esp a modo de cliente para conectarse a una red existente
    WiFi.mode(WIFI_STA);

    WiFi.onEvent(OnWiFiEvent);

    // inicia la conexión a la red
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // configura el servidor para la conexión a rabbit
    mqttClient.setServer(brokerHost, brokerPort);

    // establece el tamaño del buffer
    mqttClient.setBufferSize(2048);

    Serial.println("\nWiFi inicializado. Esperando conexion...\n");
};

bool MqttCommunication::Connect()
{
    if (!CheckWifiBrokerConnection())
        return false;

    // conectar al broker
    bool connected = mqttClient.connect(clientId, brokerUser, brokerPass);

    // imprimir que esté o no conectado
    if (!connected)
    {
        Serial.print("\n No conectado al broker: \n");
        Serial.print(mqttClient.state());
        Serial.print("\n");
        return false;
    }

    mqttClient.subscribe(enumSwitch.TopicsName(Topics::Actions).c_str());

    // devuelve el estado de la conexión
    Serial.print("\nConectado al broker\n");
    return true;
}

bool MqttCommunication::IsConnected()
{
    return mqttClient.connected();
}

bool MqttCommunication::PublishTelemetry(FullTelemetry &fullTelemetry)
{
    std::string payload;

    if (!mqttClient.connected())
    {
        Serial.print("\nMensaje no enviado, no conectado al broker:\n");
        Serial.print(mqttClient.state());
        Serial.print("\n");
        return false;
    }

    JsonDocument doc = LoadDataIntoJson(fullTelemetry);
    serializeJson(doc, payload);

    bool published = mqttClient.publish(enumSwitch.TopicsName(Topics::Telemetry).c_str(), payload.c_str());

    Serial.print("\nMensaje enviado, publicado:\n");
    Serial.print(published);

    return published;
}

bool MqttCommunication::Loop()
{
    mqttClient.loop();

    // validar conexión a wifi
    if (!CheckWifiBrokerConnection())
        return false;

    static unsigned long lastReconnAttempt = 0;
    constexpr unsigned long RECONN_INTERVAL = 5000;

    unsigned long currTime = millis();

    if (currTime - lastReconnAttempt >= RECONN_INTERVAL)
    {
        lastReconnAttempt = currTime;
        return Connect();
    }

    return false;
}

// consume los mensajes de la cola "Actions" y actua en consecuencia
bool MqttCommunication::ConsumeActions()
{
    // verifica la conexión al broker y al wifi
    if (!CheckWifiBrokerConnection())
        return false;

    // lee los mensajes de "Actions"


    // deserializa los datos para identificar los actuadores y el comando

    // organiza los comandos y establece el actuador indicado

    // ejecuta el comando

    return true;
}

void MqttCommunication::TestBrokerConnection()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi no conectado");
        return;
    }

    WiFiClient client;

    Serial.println("Probando conexion TCP con RabbitMQ...");

    if (client.connect(MQTT_HOST, MQTT_PORT, 3000))
    {
        Serial.println("Conexion TCP exitosa");
        client.stop();
    }
    else
    {
        Serial.println("No se pudo establecer conexion TCP");
    }
}

void MqttCommunication::CheckNetworks()
{
    WiFi.mode(WIFI_STA);

    int networks = WiFi.scanNetworks();

    Serial.print("Redes encontradas: ");
    Serial.println(networks);

    for (int i = 0; i < networks; i++)
    {
        Serial.print(WiFi.SSID(i));
        Serial.print(" | RSSI: ");
        Serial.print(WiFi.RSSI(i));
        Serial.print(" dBm | Canal: ");
        Serial.println(WiFi.channel(i));
    }
}