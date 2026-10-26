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
#include "../Objects/DeviceInventory.h"

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
        void SetInventory(const DeviceInventory &inventory);
        bool PublishInventory();
        bool Loop(); 
        void CheckNetworks();
        void TestBrokerConnection();
        bool TryTakeAction(ActionMessage& result);
        bool TryTakeControlMessage(std::string &message);
        bool PublishControlStatus(const std::string &payload);

    private:
        WiFiClient wifiClient;
        PubSubClient mqttClient;

        DeviceConfiguration configuration;
        DeviceTopics topics;
        bool initialized;
        DeviceInventory inventory;
        std::string inventoryPayload;
        bool inventoryAvailable;
        bool inventoryPending;

        void HandleMessage(char* topic, byte* payload, int length);
        std::optional<ActionMessage> pendingAction;
        std::optional<std::string> pendingControlMessage;
};

#endif
