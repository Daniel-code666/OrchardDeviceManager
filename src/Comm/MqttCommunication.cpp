#include "MqttCommunication.h"
#include <ArduinoJson.h>
#include <string.h>

namespace
{
    JsonConverter jsonConverter = JsonConverter();
}

// constructor
MqttCommunication::MqttCommunication()
    : wifiClient(), mqttClient(wifiClient), configuration(), initialized(false),
      inventoryAvailable(false), inventoryPending(false) {}

// Inicializa MQTT. El ciclo Wi-Fi pertenece a ProvisioningManager.
bool MqttCommunication::Init(const DeviceConfiguration &newConfiguration)
{
    if (!newConfiguration.IsValid())
        return false;

    DeviceTopics newTopics;
    if (!DeviceTopics::TryCreate(newConfiguration.deviceId, newTopics))
        return false;

    configuration = newConfiguration;
    topics = std::move(newTopics);

    mqttClient.disconnect();

    // configura el servidor para la conexión a rabbit
    mqttClient.setServer(configuration.mqttHost.c_str(), configuration.mqttPort);

    mqttClient.setCallback(
        [this](char *topic, byte *payload, unsigned int length)
        {
            HandleMessage(topic, payload, length);
        });

    // establece el tamaño del buffer
    mqttClient.setBufferSize(4096);

    initialized = true;

    Serial.println("\nMQTT inicializado. Esperando conexion Wi-Fi...\n");
    return true;
};

void MqttCommunication::Stop()
{
    mqttClient.disconnect();
    pendingAction.reset();
    pendingControlMessage.reset();
    configuration = DeviceConfiguration{};
    topics = DeviceTopics{};
    initialized = false;
    inventoryPending = inventoryAvailable;
}

void MqttCommunication::HandleMessage(char *topic, byte *payload, int length)
{
    if (topic == nullptr)
        return;

    if (topics.control == topic)
    {
        if (length <= 0 || length > 4096 || pendingControlMessage.has_value())
        {
            Serial.println("Comando de control rechazado: longitud o cola no disponible");
            return;
        }
        pendingControlMessage = std::string(
            reinterpret_cast<const char *>(payload), static_cast<size_t>(length));
        return;
    }

    if (topics.actions != topic)
    {
        Serial.println("Mensaje MQTT rechazado: topic no autorizado para este dispositivo");
        return;
    }

    ActionMessage actionMessage;

    Serial.printf("\nMensaje recibido en: %s\n", topic);

    bool success = JsonConverter::Deserialize(payload, length, actionMessage);

    if (!success)
    {
        Serial.println("Error deserializando ActionMessage");
        return;
    }

    if (!actionMessage.targetDeviceId.empty() &&
        actionMessage.targetDeviceId != configuration.deviceId)
    {
        Serial.println("Mensaje MQTT rechazado: deviceId no corresponde a este dispositivo");
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
    if (!initialized)
        return false;

    if (WiFi.status() != WL_CONNECTED)
        return false;

    if (mqttClient.connected())
        return true;

    // conectar al broker
    // Sesion limpia: los comandos no deben acumularse mientras el dispositivo esta desconectado.
    bool connected = mqttClient.connect(configuration.mqttClientId.c_str(),
                                        configuration.mqttUser.c_str(),
                                        configuration.mqttPassword.c_str(),
                                        nullptr, 0, false, nullptr, true);

    // imprimir que esté o no conectado
    if (!connected)
    {
        Serial.print("\n No conectado al broker: \n");
        Serial.print(mqttClient.state());
        Serial.print("\n");
        return false;
    }

    bool subscribed = mqttClient.subscribe(topics.actions.c_str());
    const bool controlSubscribed = mqttClient.subscribe(topics.control.c_str());
    inventoryPending = inventoryAvailable;

    // devuelve el estado de la conexión
    Serial.print("\nConectado al broker\n");

    Serial.printf(
        "Suscripcion MQTT | Topic: [%s] | Resultado: %s\n",
        topics.actions.c_str(),
        subscribed ? "OK" : "ERROR");
    Serial.printf("Suscripcion control | Topic: [%s] | Resultado: %s\n",
                  topics.control.c_str(), controlSubscribed ? "OK" : "ERROR");

    if (!subscribed || !controlSubscribed)
    {
        mqttClient.disconnect();
        Serial.println("Conexion MQTT descartada: faltan suscripciones obligatorias");
        return false;
    }
    return true;
}

bool MqttCommunication::TryTakeControlMessage(std::string &message)
{
    if (!pendingControlMessage.has_value())
        return false;
    message = std::move(*pendingControlMessage);
    pendingControlMessage.reset();
    return true;
}

bool MqttCommunication::PublishControlStatus(const std::string &payload)
{
    if (!mqttClient.connected() || payload.empty() || payload.size() > 4000)
        return false;
    return mqttClient.publish(topics.controlStatus.c_str(), payload.c_str(), true);
}

bool MqttCommunication::Loop()
{
    if (!initialized)
        return false;

    if (WiFi.status() != WL_CONNECTED)
        return false;

    if (mqttClient.connected())
    {
        const bool loopResult = mqttClient.loop();
        if (inventoryPending)
            PublishInventory();
        return loopResult;
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

    bool published = mqttClient.publish(topics.telemetry.c_str(), payload.c_str());

    Serial.print("\nMensaje enviado, publicado:\n");
    Serial.print(published);

    return published;
}

bool MqttCommunication::PublishActionResponse(const ActionResult& result)
{
    if (!mqttClient.connected())
        return false;

    JsonDocument doc = jsonConverter.ToJson(result, configuration.deviceId);

    std::string payload;
    serializeJson(doc, payload);

    bool published = mqttClient.publish(topics.actionsResponse.c_str(), payload.c_str());

    Serial.printf("Respuesta publicada: %s | Bytes: %u\n", published ? "SI" : "NO",  static_cast<unsigned>(payload.size()));

    return published;
}

void MqttCommunication::SetInventory(const DeviceInventory &newInventory)
{
    JsonDocument document = jsonConverter.InventoryToJson(newInventory);
    std::string newPayload;
    serializeJson(document, newPayload);
    const bool changed = newPayload != inventoryPayload;
    inventory = newInventory;
    inventoryPayload = std::move(newPayload);
    inventoryAvailable = !inventory.deviceId.empty() && !inventory.devices.empty() &&
                         inventoryPayload.size() <= 4000;
    inventoryPending = inventoryAvailable && (inventoryPending || changed);
}

bool MqttCommunication::PublishInventory()
{
    if (!mqttClient.connected() || !inventoryAvailable)
        return false;

    if (inventoryPayload.size() > 4000)
    {
        Serial.println("Inventario no publicado: payload demasiado grande");
        return false;
    }

    const bool published = mqttClient.publish(
        topics.inventory.c_str(), inventoryPayload.c_str(), true);
    if (published)
        inventoryPending = false;
    Serial.printf("Inventario MQTT | Topic: [%s] | Resultado: %s | Bytes: %u\n",
                  topics.inventory.c_str(), published ? "OK" : "ERROR",
                  static_cast<unsigned>(inventoryPayload.size()));
    return published;
}

void MqttCommunication::TestBrokerConnection()
{
    if (!initialized)
    {
        Serial.println("Comunicacion no inicializada");
        return;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi no conectado");
        return;
    }

    WiFiClient client;

    Serial.println("Probando conexion TCP con RabbitMQ...");

    if (client.connect(configuration.mqttHost.c_str(), configuration.mqttPort, 3000))
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
