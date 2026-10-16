#ifndef WATER_PUMP_H
#define WATER_PUMP_H

#include "Objects/Device.h"
#include "Objects/ActuatorData.h"
#include "Objects/PumpControlMode.h"
#include <string>
#include <optional>

class WaterPump : public Device
{
    public:
        explicit WaterPump(int id, int pin, std::string actuatorName);

        void Init() override;
        bool GetState();
        std::string GetControlMode();
        bool SetMode(int mode);
        bool SetState(float moisturePercentage, float drySoilPercentage, float wetSoilPercentage, std::optional<bool> state = std::nullopt);
        ActuatorData GetActuatorData();

    private:
        PumpControlMode mode = PumpControlMode::Automatic;
};

#endif