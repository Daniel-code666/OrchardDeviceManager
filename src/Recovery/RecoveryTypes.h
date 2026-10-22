#pragma once

#include <cstdint>
#include <string>

enum class RecoveryAction : uint8_t
{
    RevokeMqtt,
    Unlink
};

enum class RecoveryCommandResult : uint8_t
{
    Applied,
    InvalidState,
    InvalidContract,
    AuthenticationFailed,
    ReplayRejected,
    StorageError
};

struct RecoveryRecord
{
    static constexpr uint16_t CURRENT_VERSION = 1;

    uint16_t version = CURRENT_VERSION;
    uint32_t lastCommandVersion = 0;
    std::string lastRequestId;
    bool mqttRevoked = false;
    bool unlinkPending = false;

    bool IsValid() const;
};

struct RecoveryCommand
{
    static constexpr uint16_t CONTRACT_VERSION = 1;

    uint16_t contractVersion = CONTRACT_VERSION;
    std::string deviceId;
    std::string requestId;
    uint32_t commandVersion = 0;
    RecoveryAction action = RecoveryAction::RevokeMqtt;
    std::string authenticationTag;
};

const char *ToString(RecoveryAction action);
const char *ToString(RecoveryCommandResult result);

