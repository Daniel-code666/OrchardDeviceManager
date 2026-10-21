#ifndef MQTT_COMMUNICATION_H
#define MQTT_COMMUNICATION_H

#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include "../Secrets.h"
#include "../Objects/FullTelemetry.h"
#include "Utils/JsonConverter.h"
#include "ActionDispatcher/ActionDispatcher.h"
#include "Objects/ActionResult.h"

class MqttCommunication
{
    public:
        explicit MqttCommunication(const char* brokerHost, uint16_t brokerPort, const char* clientId, const char* brokerUser, const char* brokerPass);
        
        void Init();
        bool Connect();
        bool IsConnected();
        bool PublishTelemetry(FullTelemetry& fullTelemetry);
        bool PublishActionResponse(const ActionResult& result);
        bool Loop(); 
        void CheckNetworks();
        void TestBrokerConnection();
        bool TryTakeAction(ActionMessage& result);

    private:
        WiFiClient wifiClient;
        PubSubClient mqttClient;

        const char* brokerHost;
        const uint16_t brokerPort;
        const char* clientId;
        const char* brokerUser;
        const char* brokerPass;

        void HandleMessage(char* topic, byte* payload, int length);
        std::optional<ActionMessage> pendingAction;
};

#endif