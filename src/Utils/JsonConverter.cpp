#include "JsonConverter.h"

namespace
{
    EnumsSwitch enumSwitch = EnumsSwitch();
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

    const char* command = json["command"];
    JsonArrayConst params = json["params"].as<JsonArrayConst>();

    if (command == nullptr || params.isNull())
        return false;

    ActionMessage parsed{};

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
        if (!item["deviceId"].is<int>() || !item["deviceType"].is<const char*>())
            return false;

        ParamsBody param{};

        param.deviceId = item["deviceId"].as<int>();

        const char* deviceType = item["deviceType"].as<const char*>();

        std::optional<DeviceType> deviceTypeEnum = enumSwitch.DeviceTypeFromString(std::string(deviceType));

        if (!deviceTypeEnum.has_value())
            return false;

        param.deviceType = deviceTypeEnum.value();

        if (item["state"].is<bool>())
            param.state = item["state"].as<bool>();

        if (item["value"].is<float>() || item["value"].is<int>())
            param.value = item["value"].as<float>();

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

JsonDocument JsonConverter::ToJson(const ActionResult& result)
{
    // establece el objeto para enviar al broker
    JsonDocument doc;
    doc["success"] = result.success;
    // organiza el objeto json a partir del objeto fullTelemetry
    JsonDocument telemetryDoc = LoadDataIntoJson(result.telemetry);
    doc["telemetry"] = telemetryDoc.as<JsonVariantConst>();
    return doc;
}