#include "MqttCommunication.h"
#include "Objects/Topics.h"
#include "../Utils/EnumsSwitch.h"

namespace
{
    EnumsSwitch enumSwitch = EnumsSwitch();
}

MqttCommunication::MqttCommunication(const char *brokerHost, uint16_t brokerPort, const char *clientId,
                                     const char *brokerUser, const char *brokerPass) : wifiClient(), mqttClient(wifiClient), brokerHost(brokerHost), brokerPort(brokerPort), clientId(clientId),
                                                                                       brokerUser(brokerUser), brokerPass(brokerPass) {};

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

    Serial.println("\nWiFi inicializado. Esperando conexion...\n");
};

bool MqttCommunication::Connect()
{
    // validar conexión a wifi
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.print("\nConexión a mqtt, validación de wifi: no conectado\n");
        return false;
    }

    // validar conexión al broker
    if (mqttClient.connected())
    {
        Serial.print("\nConexión a mqtt, validación de conexión a broker: conectado\n");
        return true;
    }

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

    // devuelve el estado de la conexión
    Serial.print("\nConectado al broker\n");
    return true;
}

bool MqttCommunication::IsConnected()
{
    return mqttClient.connected();
}

bool MqttCommunication::PublishTelemetry(FullTelemetry fullTelemetry)
{
    if (!mqttClient.connected())
    {
        Serial.print("\nMensaje no enviado, no conectado al broker:\n");
        Serial.print(mqttClient.state());
        Serial.print("\n");
        return false;
    }

    bool published = mqttClient.publish(enumSwitch.TopicsName(Topics::Telemetry).c_str(), "Mensaje enviado al tópico");

    Serial.print("\nMensaje enviado, publicado:\n");
    Serial.print(published);

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