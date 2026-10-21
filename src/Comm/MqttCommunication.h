#ifndef MQTT_COMMUNICATION_H
#define MQTT_COMMUNICATION_H

#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include "../Secrets.h"
#include "../Objects/FullTelemetry.h"

class MqttCommunication
{
    public:
        explicit MqttCommunication(const char* brokerHost, uint16_t brokerPort, const char* clientId, const char* brokerUser, const char* brokerPass);
        
        void Init();
        bool Connect();
        bool IsConnected();
        bool PublishTelemetry(FullTelemetry& fullTelemetry);
        bool ConsumeActions();
        bool Loop(); 
        void CheckNetworks();
        void TestBrokerConnection();

    private:
        WiFiClient wifiClient;
        PubSubClient mqttClient;

        const char* brokerHost;
        const uint16_t brokerPort;
        const char* clientId;
        const char* brokerUser;
        const char* brokerPass;
};

#endif