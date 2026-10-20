#include "EnumsSwitch.h"

std::string EnumsSwitch::MeasurementTypesName(MeasurementType measurementType)
{
    switch (measurementType)
    {
        case MeasurementType::SoilTemperature: return "SoilTemperature";
        case MeasurementType::AirTemperature: return "AirTemperature";
        case MeasurementType::AirHumidity: return "AirHumidity";
        case MeasurementType::Illuminance: return "Illuminance";
        case MeasurementType::SoilMoisture: return "SoilMoisture";
        default: return "Unknow";
    }
}

std::string EnumsSwitch::MeasurementUnitsName(MeasurementUnits measurementUnits)
{
    switch (measurementUnits)
    {
        case MeasurementUnits::Celsius: return "Celsius";
        case MeasurementUnits::Lux: return "Lux";
        case MeasurementUnits::Percentage: return "Percentage";
        default: return "Unknow";
    }
}

std::string EnumsSwitch::ConnectionTypeName(ConnectionType connType)
{
    switch (connType)
    {
        case ConnectionType::Analog: return "Analog";
        case ConnectionType::Digital: return "Digital";
        case ConnectionType::I2C: return "I2C";
        case ConnectionType::OneWire: return "OneWire";
        default: return "Unknow";
    }
}

std::string EnumsSwitch::PumpControlModeName(PumpControlMode pumpControlMode)
{
    switch (pumpControlMode)
    {
        case PumpControlMode::Automatic: return "Automatic";
        case PumpControlMode::Manual: return "Manual";
        default: return "Unknow";
    }
}

std::string EnumsSwitch::TopicsName(Topics topic)
{
    switch(topic)
    {
        case Topics::Telemetry: return "Telemetry";
        case Topics::Actions: return "Actions";
        default: return "Unknow";
    }
}

DeviceIdByType EnumsSwitch::OrganizeIdsByType(ActionMessage actionMessage)
{
    DeviceIdByType deviceIdByType;
    std::vector<ParamsBody> paramsData = { actionMessage.params };

    for(int i = 0; i < paramsData.size(); i++)
    {
        if (paramsData[i].deviceType == DeviceType::Sensor)
        {
            deviceIdByType.sensorIds.push_back(paramsData[i].deviceId);
        }

        if (paramsData[i].deviceType == DeviceType::Actuator)
        {
            deviceIdByType.actuatorIds.push_back(paramsData[i].deviceId);
        }
    }

    return deviceIdByType;
}