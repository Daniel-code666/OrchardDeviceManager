#include "RemoteControlManager.h"

#include "../DeviceProvisioning/ProvisioningTypes.h"
#include "../Ota/OtaCommandAuthentication.h"
#include "../Ota/OtaControlJson.h"
#include "../Recovery/RecoveryJson.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ctime>

namespace
{
    constexpr time_t MINIMUM_VALID_TIME = 1700000000;
    constexpr unsigned long STATUS_HEARTBEAT_MS = 15000;

    int64_t DaysFromCivil(int year, unsigned month, unsigned day)
    {
        year -= month <= 2;
        const int era = (year >= 0 ? year : year - 399) / 400;
        const unsigned yoe = static_cast<unsigned>(year - era * 400);
        const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
        const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
    }
}

RemoteControlManager::RemoteControlManager(
    MqttCommunication &mqtt, FirmwareUpdateManager &firmware,
    MqttCredentialProvisioningManager &credentials,
    DeviceProvisioningManager &provisioning,
    DeviceRecoveryManager &recovery, Sensors &deviceSensors)
    : communication(mqtt), firmwareManager(firmware), credentialManager(credentials),
      provisioningManager(provisioning), recoveryManager(recovery), sensors(deviceSensors),
      lastRuntimeState(OtaRuntimeState::Idle), lastProgress(255), lastStatusAt(0),
      initialized(false), clockConfigured(false)
{
}

bool RemoteControlManager::Begin(const std::string &stableDeviceId)
{
    if (!IsValidProvisioningIdentifier(stableDeviceId))
        return false;
    deviceId = stableDeviceId;
    initialized = true;
    lastRuntimeState = firmwareManager.GetState();
    QueuePersistentOtaStatus();
    return true;
}

void RemoteControlManager::ProcessMessage(const std::string &message, unsigned long now)
{
    if (!initialized || message.empty() || message.size() > 4096)
        return;
    JsonDocument document;
    if (deserializeJson(document, message) != DeserializationError::Ok)
        return;
    const char *type = document["type"];
    if (type == nullptr)
        return;
    const std::string messageType = type;
    if (messageType == "ota_update_request")
        ProcessOtaRequest(message, now);
    else if (messageType == "recovery_command")
        ProcessRecoveryRequest(message);
}

void RemoteControlManager::ProcessOtaRequest(const std::string &message, unsigned long now)
{
    RemoteOtaRequest request;
    if (!OtaControlJson::DeserializeRemoteRequest(message, request))
    {
        QueueOtaStatus("unknown", "unknown-release", "Failed", 0, "",
                       "invalid_contract");
        return;
    }
    if (!provisioningManager.IsProvisioned() || request.contractVersion != 1 ||
        request.deviceId != deviceId || !IsValidProvisioningIdentifier(request.deviceId) ||
        !IsValidProvisioningIdentifier(request.requestId, 64) ||
        !IsValidProvisioningIdentifier(request.releaseId, 64) || request.commandVersion == 0 ||
        request.manifestUrl.empty() || request.manifestUrl.size() > 512)
    {
        QueueOtaStatus(request.requestId, request.releaseId, "Failed", 0, "",
                       "invalid_contract");
        return;
    }

    const OtaRecord &record = firmwareManager.GetRecord();
    if (request.commandVersion <= record.commandVersion || request.requestId == record.requestId)
    {
        QueueOtaStatus(request.requestId, request.releaseId, "Failed", 0, "",
                       "replay_rejected");
        return;
    }

    time_t expiresAt = 0;
    const time_t currentTime = time(nullptr);
    if (!ParseUtc(request.expiresAt, expiresAt) || currentTime < MINIMUM_VALID_TIME)
    {
        QueueOtaStatus(request.requestId, request.releaseId, "Failed", 0, "",
                       "clock_unavailable");
        return;
    }
    if (currentTime >= expiresAt)
    {
        QueueOtaStatus(request.requestId, request.releaseId, "Failed", 0, "",
                       "request_expired");
        return;
    }

    std::string key;
    if (!credentialManager.TryGetLocalBootstrapKey(key) ||
        !OtaCommandAuthentication::Verify(request, key))
    {
        key.assign(key.size(), '0');
        QueueOtaStatus(request.requestId, request.releaseId, "Failed", 0, "",
                       "authentication_failed");
        return;
    }
    key.assign(key.size(), '0');

    const OtaStartResult result = firmwareManager.StartUpdate(
        request.manifestUrl, sensors.IsWaterPumpActive(), request.releaseId,
        request.requestId, request.commandVersion);
    if (result != OtaStartResult::Started)
    {
        QueueOtaStatus(request.requestId, request.releaseId, "Failed", 0, "",
                       ToString(result));
        return;
    }
    lastRuntimeState = firmwareManager.GetState();
    lastProgress = 0;
    lastStatusAt = now;
    QueuePersistentOtaStatus();
}

void RemoteControlManager::ProcessRecoveryRequest(const std::string &message)
{
    RecoveryCommand command;
    JsonDocument status;
    status["type"] = "recovery_command_result";
    status["contractVersion"] = 1;
    status["deviceId"] = deviceId;
    if (!RecoveryJson::DeserializeCommand(message, command))
    {
        status["requestId"] = "unknown";
        status["result"] = "invalid_contract";
    }
    else
    {
        status["requestId"] = command.requestId;
        std::string key;
        const RecoveryCommandResult result = credentialManager.TryGetLocalBootstrapKey(key)
                                                 ? recoveryManager.ProcessCommand(command, key)
                                                 : RecoveryCommandResult::InvalidState;
        key.assign(key.size(), '0');
        status["result"] = ToString(result);
    }
    serializeJson(status, pendingStatus);
}

void RemoteControlManager::Loop(unsigned long now)
{
    if (!initialized)
        return;
    if (!clockConfigured && WiFi.status() == WL_CONNECTED)
    {
        configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
        clockConfigured = true;
    }

    const OtaRuntimeState state = firmwareManager.GetState();
    const uint8_t progress = firmwareManager.GetProgressPercent();
    const bool progressChanged = lastProgress == 255 || progress == 100 ||
                                 progress >= static_cast<uint8_t>(lastProgress + 5);
    const bool changed = state != lastRuntimeState || progressChanged;
    if (pendingStatus.empty() &&
        (changed || static_cast<unsigned long>(now - lastStatusAt) >= STATUS_HEARTBEAT_MS))
    {
        lastRuntimeState = state;
        lastProgress = progress;
        lastStatusAt = now;
        QueuePersistentOtaStatus();
    }

    if (!pendingStatus.empty() && communication.PublishControlStatus(pendingStatus))
        pendingStatus.clear();
}

void RemoteControlManager::QueuePersistentOtaStatus()
{
    const OtaRecord &record = firmwareManager.GetRecord();
    if (record.requestId.empty())
        return;
    const OtaRuntimeState state = firmwareManager.GetState();
    const char *status = "Pending";
    if (state == OtaRuntimeState::Downloading) status = "Downloading";
    else if (state == OtaRuntimeState::PendingValidation ||
             state == OtaRuntimeState::ReadyToRestart) status = "PendingValidation";
    else if (record.state == OtaPersistentState::Confirmed) status = "Confirmed";
    else if (state == OtaRuntimeState::Failed || record.state == OtaPersistentState::Failed)
        status = "Failed";
    uint8_t progress = firmwareManager.GetProgressPercent();
    if (record.state == OtaPersistentState::PendingValidation ||
        record.state == OtaPersistentState::Confirmed)
        progress = 100;
    QueueOtaStatus(record.requestId, record.releaseId, status, progress,
                   record.targetVersion, record.failureCode.c_str());
}

void RemoteControlManager::QueueOtaStatus(
    const std::string &requestId, const std::string &releaseId, const char *status,
    uint8_t progress, const std::string &targetVersion, const char *failureCode)
{
    JsonDocument document;
    document["type"] = "ota_update_status";
    document["contractVersion"] = 1;
    document["deviceId"] = deviceId;
    document["requestId"] = requestId;
    document["releaseId"] = releaseId;
    document["status"] = status;
    document["progressPercent"] = progress;
    document["currentVersion"] = firmwareManager.GetCurrentVersion();
    document["targetVersion"] = targetVersion;
    if (failureCode != nullptr && failureCode[0] != '\0')
        document["failureCode"] = failureCode;
    else
        document["failureCode"] = nullptr;
    pendingStatus.clear();
    serializeJson(document, pendingStatus);
}

bool RemoteControlManager::ParseUtc(const std::string &value, time_t &timestamp)
{
    if (value.size() != 20 || value[4] != '-' || value[7] != '-' ||
        value[10] != 'T' || value[13] != ':' || value[16] != ':' || value[19] != 'Z')
        return false;
    const auto number = [&value](size_t offset, size_t count, int &result)
    {
        result = 0;
        for (size_t index = 0; index < count; ++index)
        {
            const char character = value[offset + index];
            if (character < '0' || character > '9') return false;
            result = result * 10 + character - '0';
        }
        return true;
    };
    int year, month, day, hour, minute, second;
    if (!number(0, 4, year) || !number(5, 2, month) || !number(8, 2, day) ||
        !number(11, 2, hour) || !number(14, 2, minute) || !number(17, 2, second) ||
        year < 2024 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour > 23 || minute > 59 || second > 59)
        return false;
    timestamp = static_cast<time_t>(DaysFromCivil(year, month, day) * 86400LL +
                                    hour * 3600 + minute * 60 + second);
    return true;
}

