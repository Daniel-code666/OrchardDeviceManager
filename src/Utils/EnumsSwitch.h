#ifndef ENUMS_SWITCH_H
#define ENUMS_SWITCH_H

#include <string>
#include "../Objects/MeasurementTypes.h"
#include "../Objects/MeasurementUnits.h"
#include "../Objects/ConnectionType.h"
#include "../Objects/PumpControlMode.h"
#include "../Objects/Topics.h"
#include "../Objects/ActionMessage.h"
#include "../Objects/Commands.h"
#include <vector>
#include "Objects/DeviceType.h"
#include "Objects/DeviceIdByType.h"
#include <optional>

class EnumsSwitch
{
    public:
        std::string MeasurementTypesName(MeasurementType measurementType);
        std::string MeasurementUnitsName(MeasurementUnits measurementName);
        std::string ConnectionTypeName(ConnectionType connType);
        std::string PumpControlModeName(PumpControlMode pumpControlMode);
        std::string TopicsName(Topics topic);
        std::string CommandName(Commands command);
        std::string CommandNameByString(std::string command);
        std::optional<Commands> CommandFromString(const std::string& command);
        std::string DeviceTypeName(DeviceType type);
        std::optional<DeviceType> DeviceTypeFromString(const std::string& type);
        DeviceIdByType OrganizeIdsByType(ActionMessage& actionMessage);
};

#endif