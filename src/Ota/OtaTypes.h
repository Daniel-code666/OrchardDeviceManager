#pragma once

#include <cstdint>
#include <string>

enum class OtaPersistentState : uint8_t
{
    Idle = 0,
    Downloading = 1,
    PendingValidation = 2,
    Confirmed = 3,
    Failed = 4
};

enum class OtaRuntimeState : uint8_t
{
    Idle,
    Downloading,
    PendingValidation,
    ReadyToRestart,
    Failed
};

enum class OtaStartResult : uint8_t
{
    Started,
    Busy,
    WifiUnavailable,
    UnsafeActuatorState,
    InvalidUrl,
    ManifestUnavailable,
    InvalidManifest,
    AuthenticationFailed,
    IncompatibleBoard,
    VersionRejected,
    StorageError,
    DownloadUnavailable,
    ImageRejected
};

struct OtaRecord
{
    static constexpr uint16_t CURRENT_VERSION = 1;

    uint16_t recordVersion = CURRENT_VERSION;
    OtaPersistentState state = OtaPersistentState::Idle;
    std::string releaseId;
    std::string targetVersion;
    std::string failureCode;

    bool IsValid() const;
};

struct FirmwareManifest
{
    static constexpr uint16_t CONTRACT_VERSION = 1;

    uint16_t contractVersion = CONTRACT_VERSION;
    std::string releaseId;
    std::string version;
    std::string board;
    uint32_t size = 0;
    std::string sha256;
    std::string firmwareUrl;
    std::string signatureHex;
};

const char *ToString(OtaPersistentState state);
const char *ToString(OtaRuntimeState state);
const char *ToString(OtaStartResult result);

