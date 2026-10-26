#pragma once

#include "ConnectionType.h"
#include "DeviceType.h"
#include "MeasurementTypes.h"
#include "MeasurementUnits.h"

#include <cstdint>
#include <string>
#include <vector>

struct DeviceInventoryItem
{
    int localDeviceId = 0;
    std::string name;
    DeviceType deviceType = DeviceType::Sensor;
    bool hasEsp32Pin = false;
    uint8_t esp32Pin = 0;
    ConnectionType connectionType = ConnectionType::Digital;
    bool hasMeasurement = false;
    MeasurementType measurementType = MeasurementType::SoilTemperature;
    MeasurementUnits measurementUnit = MeasurementUnits::Celsius;
    bool available = false;
};

struct DeviceInventory
{
    static constexpr uint16_t CONTRACT_VERSION = 1;

    std::string deviceId;
    std::string firmwareVersion;
    std::vector<DeviceInventoryItem> devices;
};

