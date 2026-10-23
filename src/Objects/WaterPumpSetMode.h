#ifndef WATER_PUMP_SET_MODE_H
#define WATER_PUMP_SET_MODE_H

#include <vector>
#include <optional>

struct WaterPumpSetMode
{
    int actuatorId;
    int option;
    std::optional<bool> state = std::nullopt;
};

#endif