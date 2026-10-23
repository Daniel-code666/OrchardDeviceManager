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

std::string EnumsSwitch::CommandName(Commands command)
{
    switch (command)
    {
        case Commands::GetSensorOrActuatorDataById: return "GetSensorOrActuatorDataById";
        case Commands::SetWaterPumpControlMode: return "SetWaterPumpControlMode";
        case Commands::SetWaterPumpState: return "SetWaterPumpState";
        default: return "Unknow";
    }
}

std::string EnumsSwitch::CommandNameByString(std::string command)
{
    if (command == CommandName(Commands::GetSensorOrActuatorDataById))
        return "GetSensorOrActuatorDataById";

    if (command == CommandName(Commands::SetWaterPumpControlMode))
        return "SetWaterPumpControlMode";

    if (command == CommandName(Commands::SetWaterPumpState))
        return "SetWaterPumpState";

    return "Unknown";
}

std::optional<Commands> EnumsSwitch::CommandFromString(const std::string& command)
{
    if (command == CommandName(Commands::GetSensorOrActuatorDataById))
        return Commands::GetSensorOrActuatorDataById;

    if (command == CommandName(Commands::SetWaterPumpControlMode))
        return Commands::SetWaterPumpControlMode;

    if (command == CommandName(Commands::SetWaterPumpState))
        return Commands::SetWaterPumpState;

    return std::nullopt;
}

std::string EnumsSwitch::DeviceTypeName(DeviceType type)
{
    switch (type)
    {
        case DeviceType::Sensor: return "sensor";
        case DeviceType::Actuator: return "actuator";
        default: return "Unknow";
    }
}

std::optional<DeviceType> EnumsSwitch::DeviceTypeFromString(const std::string& type)
{
    if (type == DeviceTypeName(DeviceType::Sensor))
        return DeviceType::Sensor;
    if (type == DeviceTypeName(DeviceType::Actuator))
        return DeviceType::Actuator;
    
    return std::nullopt;
}

DeviceIdByType EnumsSwitch::OrganizeIdsByType(ActionMessage& actionMessage)
{
    DeviceIdByType deviceIdByType;

    for (const ParamsBody& param : actionMessage.params)
    {
        if (param.deviceType == DeviceType::Sensor)
        {
            deviceIdByType.sensorIds.push_back(param.deviceId);
        }
        else if (param.deviceType == DeviceType::Actuator)
        {
            deviceIdByType.actuatorIds.push_back(param.deviceId);
        }
    }

    return deviceIdByType;
}

std::vector<WaterPumpSetMode> EnumsSwitch::SetDataForWaterPumpControlModeAction(ActionMessage& actionMessage)
{
    std::vector<WaterPumpSetMode> result;
    WaterPumpSetMode data;

    for(const ParamsBody& param : actionMessage.params)
    {
        if (param.deviceType != DeviceType::Actuator)
            continue;

        WaterPumpSetMode data{};

        data.actuatorId = param.deviceId;
        data.option = param.value;

        result.push_back(data);
    }

    return result;
}
