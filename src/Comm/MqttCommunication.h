#ifndef MQTT_COMMUNICATION_H
#define MQTT_COMMUNICATION_H

#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include "../Secrets.h"

class MqttCommunication
{
    public:
        explicit MqttCommunication(const char* brokerHost, uint16_t brokerPort, const char* clientId);
        
        void init();
        bool connect();
        bool isConnected();
        void publishTelemetry();
        void loop(); 
        void checkNetworks();

    private:
        WiFiClient wifiClient;
        PubSubClient mqttClient;

        const char* brokerHost;
        const uint16_t brokerPort;
        const char* clientId;
};

#endif