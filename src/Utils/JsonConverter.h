#ifndef JSON_CONVERTER_H
#define JSON_CONVERTER_H

#include <ArduinoJson.h>
#include <cstdint>
#include <cstddef>
#include "Objects/ActionMessage.h"
#include "Utils/EnumsSwitch.h"
#include <string>
#include "Objects/FullTelemetry.h"
#include "Objects/ActionResult.h"

class JsonConverter
{
    public:
        template <typename T>
        static bool Deserialize(const uint8_t* payload, size_t length, T& result);
        JsonDocument LoadDataIntoJson(const FullTelemetry &fullTelemetry);
        JsonDocument ToJson(const ActionResult& result);
    private:
        static bool Convert(JsonVariantConst json, ActionMessage& result);
};

#endif