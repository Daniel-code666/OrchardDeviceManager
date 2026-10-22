#ifndef MQTT_COMMUNICATION_H
#define MQTT_COMMUNICATION_H

#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <string>
#include "../Configuration/DeviceConfiguration.h"
#include "DeviceTopics.h"
#include "../Objects/FullTelemetry.h"
#include "Utils/JsonConverter.h"
#include "ActionDispatcher/ActionDispatcher.h"
#include "Objects/ActionResult.h"

class MqttCommunication
{
    public:
        MqttCommunication();
        
        bool Init(const DeviceConfiguration& configuration);
        void Stop();
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

        DeviceConfiguration configuration;
        DeviceTopics topics;
        bool initialized;

        void HandleMessage(char* topic, byte* payload, int length);
        std::optional<ActionMessage> pendingAction;
};

#endif
