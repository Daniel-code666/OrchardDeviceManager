#include "MqttCommunication.h"

MqttCommunication::MqttCommunication(const char *brokerHost, uint16_t brokerPort, const char *clientId) : wifiClient(), mqttClient(wifiClient),
                                                                                                          brokerHost(brokerHost), brokerPort(brokerPort), clientId(clientId) {};

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
void MqttCommunication::init()
{
    // Establece el modo de wifi del esp a modo de cliente para conectarse a una red existente
    WiFi.mode(WIFI_STA);

    WiFi.onEvent(OnWiFiEvent);

    // inicia la conexión a la red
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // configura el servidor para la conexión a rabbit
    mqttClient.setServer(brokerHost, brokerPort);

    Serial.println("WiFi inicializado. Esperando conexion...");
};

void MqttCommunication::checkNetworks()
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