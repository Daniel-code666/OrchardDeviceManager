#include "MqttCommunication.h"
#include "Objects/Topics.h"
#include "../Utils/EnumsSwitch.h"
#include <ArduinoJson.h>
#include <string.h>

namespace
{
    EnumsSwitch enumSwitch = EnumsSwitch();
    JsonConverter jsonConverter = JsonConverter();
}

// constructor
MqttCommunication::MqttCommunication(const char *brokerHost, uint16_t brokerPort, const char *clientId,
                                     const char *brokerUser, const char *brokerPass) : wifiClient(), mqttClient(wifiClient), brokerHost(brokerHost), brokerPort(brokerPort), clientId(clientId),
                                                                                       brokerUser(brokerUser), brokerPass(brokerPass) {};

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

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.print("\nWiFi conectado | IP: \n");
        Serial.println(WiFi.localIP());
    }

    // configura el servidor para la conexión a rabbit
    mqttClient.setServer(brokerHost, brokerPort);

    mqttClient.setCallback(
        [this](char *topic, byte *payload, unsigned int length)
        {
            HandleMessage(topic, payload, length);
        });

    // establece el tamaño del buffer
    mqttClient.setBufferSize(2048);

    Serial.println("\nWiFi inicializado. Esperando conexion...\n");
};

void MqttCommunication::HandleMessage(char *topic, byte *payload, int length)
{
    ActionMessage actionMessage;

    Serial.printf("\nMensaje recibido en: %s\n", topic);

    bool success = JsonConverter::Deserialize(payload, length, actionMessage);

    if (!success)
    {
        Serial.println("Error deserializando ActionMessage");
        return;
    }

    Serial.println("ActionMessage deserializado correctamente");

    if (pendingAction.has_value())
    {
        Serial.println("Comando rechazado: hay una accion pendiente");
        return;
    }

    pendingAction = std::move(actionMessage);
}

bool MqttCommunication::TryTakeAction(ActionMessage& result)
{
    if (!pendingAction.has_value())
        return false;

    result = std::move(*pendingAction);
    pendingAction.reset();

    return true;
}

bool MqttCommunication::Connect()
{
    if (WiFi.status() != WL_CONNECTED)
        return false;

    if (mqttClient.connected())
        return true;

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

    bool subscribed = mqttClient.subscribe(enumSwitch.TopicsName(Topics::Actions).c_str());

    // devuelve el estado de la conexión
    Serial.print("\nConectado al broker\n");

    Serial.printf(
        "Suscripcion MQTT | Topic: [%s] | Resultado: %s\n",
        enumSwitch.TopicsName(Topics::Actions).c_str(),
        subscribed ? "OK" : "ERROR");

    return true;
}

bool MqttCommunication::Loop()
{
    if (WiFi.status() != WL_CONNECTED)
        return false;

    if (mqttClient.connected())
    {
        return mqttClient.loop();
    }

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

    JsonDocument doc = jsonConverter.LoadDataIntoJson(fullTelemetry);
    serializeJson(doc, payload);

    bool published = mqttClient.publish(enumSwitch.TopicsName(Topics::Telemetry).c_str(), payload.c_str());

    Serial.print("\nMensaje enviado, publicado:\n");
    Serial.print(published);

    return published;
}

bool MqttCommunication::PublishActionResponse(const ActionResult& result)
{
    if (!mqttClient.connected())
        return false;

    JsonDocument doc = jsonConverter.ToJson(result);

    std::string payload;
    serializeJson(doc, payload);

    std::string topic = enumSwitch.TopicsName(Topics::ActionsResponse);

    bool published = mqttClient.publish(topic.c_str(), payload.c_str());

    Serial.printf("Respuesta publicada: %s | Bytes: %u\n", published ? "SI" : "NO",  static_cast<unsigned>(payload.size()));

    return published;
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