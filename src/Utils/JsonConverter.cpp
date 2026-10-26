#include "JsonConverter.h"
#include <cstring>

namespace
{
    EnumsSwitch enumSwitch = EnumsSwitch();

    bool IsValidRequestId(const char *value)
    {
        if (value == nullptr || value[0] == '\0' || strlen(value) > 64)
            return false;
        for (const char *character = value; *character != '\0'; ++character)
        {
            const bool alphaNumeric = (*character >= 'a' && *character <= 'z') ||
                                      (*character >= 'A' && *character <= 'Z') ||
                                      (*character >= '0' && *character <= '9');
            if (!alphaNumeric && *character != '-' && *character != '_' &&
                *character != '.' && *character != ':')
                return false;
        }
        return true;
    }
}

template <typename T>
bool JsonConverter::Deserialize(const uint8_t* payload, size_t length, T& result)
{
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, payload, length);

    if (error)
        return false;

    return Convert(doc.as<JsonVariantConst>(), result);
}

template bool JsonConverter::Deserialize<ActionMessage>(
    const uint8_t* payload,
    size_t length,
    ActionMessage& result
);

bool JsonConverter::Convert(JsonVariantConst json, ActionMessage &result)
{
    if (!json.is<JsonObjectConst>())
        return false;

    if (!json["requestId"].is<const char*>())
        return false;

    const char *type = json["type"] | "";
    const int contractVersion = json["contractVersion"] | 0;
#if ENABLE_LOCAL_PROVISIONING_MOCK
    const bool legacyContract = type[0] == '\0' && contractVersion == 0;
#else
    const bool legacyContract = false;
#endif
    if (!legacyContract && (std::string(type) != "device_action" || contractVersion != 1))
        return false;

    const char* command = json["command"];
    JsonArrayConst params = json["params"].as<JsonArrayConst>();
    const char* requestId = json["requestId"].as<const char*>();

    if (!IsValidRequestId(requestId) || params.size() == 0 || params.size() > 16)
        return false;

    if (command == nullptr || params.isNull())
        return false;

    ActionMessage parsed{};

    parsed.requestId = requestId;
    parsed.contractVersion = legacyContract ? 1 : static_cast<uint16_t>(contractVersion);
    const char *targetDeviceId = json["deviceId"] | "";
    parsed.targetDeviceId = targetDeviceId;

    std::optional<Commands> commandEnum = enumSwitch.CommandFromString(std::string(command));

    if (!commandEnum.has_value())
        return false;

    parsed.command = commandEnum.value();

    for (JsonVariantConst item : params)
    {
        // Cada elemento de params es un objeto JSON
        if (!item.is<JsonObjectConst>())
            return false;

        // Validar tipos
        if (!item["deviceId"].is<int>())
            return false;

        ParamsBody param{};

        param.deviceId = item["deviceId"].as<int>();

        const char* deviceType = item["deviceType"] | "";
        if (deviceType[0] != '\0')
        {
            std::optional<DeviceType> deviceTypeEnum =
                enumSwitch.DeviceTypeFromString(std::string(deviceType));
            if (!deviceTypeEnum.has_value())
                return false;
            param.deviceType = deviceTypeEnum.value();
        }
        else
        {
            param.deviceType = parsed.command == Commands::GetSensorOrActuatorDataById &&
                                       param.deviceId != 6
                                   ? DeviceType::Sensor
                                   : DeviceType::Actuator;
        }

        if (item["state"].is<bool>())
            param.state = item["state"].as<bool>();

        if (parsed.command == Commands::SetWaterPumpState && !item["state"].is<bool>())
            return false;

        if (item["value"].is<float>() || item["value"].is<int>())
            param.value = item["value"].as<float>();

        if (parsed.command == Commands::SetWaterPumpControlMode &&
            !(item["value"].is<float>() || item["value"].is<int>()))
            return false;

        parsed.params.push_back(param);
    }

    result = std::move(parsed);

    return true;
}

// crea el json para luego parcearlo
JsonDocument JsonConverter::LoadDataIntoJson(const FullTelemetry& fullTelemetry)
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

JsonDocument JsonConverter::ToJson(const ActionResult& result, const std::string &deviceId)
{
    // establece el objeto para enviar al broker
    JsonDocument doc;
    doc["type"] = "device_action_result";
    doc["contractVersion"] = 1;
    doc["requestId"] = result.requestId;
    doc["deviceId"] = deviceId;
    doc["command"] = enumSwitch.CommandName(result.command);
    doc["success"] = result.success;
    // organiza el objeto json a partir del objeto fullTelemetry
    JsonDocument telemetryDoc = LoadDataIntoJson(result.telemetry);
    doc["telemetry"] = telemetryDoc.as<JsonVariantConst>();
    return doc;
}

JsonDocument JsonConverter::InventoryToJson(const DeviceInventory &inventory)
{
    JsonDocument document;
    document["type"] = "device_inventory";
    document["contractVersion"] = DeviceInventory::CONTRACT_VERSION;
    document["deviceId"] = inventory.deviceId;
    document["firmwareVersion"] = inventory.firmwareVersion;
    JsonArray devices = document["devices"].to<JsonArray>();
    for (const DeviceInventoryItem &item : inventory.devices)
    {
        JsonObject entry = devices.add<JsonObject>();
        entry["localDeviceId"] = item.localDeviceId;
        entry["name"] = item.name;
        entry["deviceType"] = item.deviceType == DeviceType::Sensor ? "Sensor" : "Actuator";
        if (item.hasEsp32Pin)
            entry["esp32Pin"] = item.esp32Pin;
        entry["connectionType"] = enumSwitch.ConnectionTypeName(item.connectionType);
        if (item.hasMeasurement)
        {
            entry["measurementType"] = enumSwitch.MeasurementTypesName(item.measurementType);
            entry["measurementUnit"] = enumSwitch.MeasurementUnitsName(item.measurementUnit);
        }
        entry["available"] = item.available;
    }
    return document;
}
