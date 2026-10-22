#pragma once

#include "OtaTypes.h"
#include "../Configuration/ConfigurationManager.h"

#include <HTTPClient.h>
#include <WiFiClient.h>
#include <string>

class FirmwareUpdateManager
{
public:
    explicit FirmwareUpdateManager(ConfigurationManager &configurationManager);

    bool Begin(const std::string &currentVersion);
    OtaStartResult StartUpdate(const std::string &manifestUrl, bool actuatorIsActive);
    void Loop(unsigned long now, bool localSystemHealthy);

    OtaRuntimeState GetState() const;
    uint8_t GetProgressPercent() const;

private:
    static constexpr size_t MAX_MANIFEST_BYTES = 4096;
    static constexpr unsigned long DOWNLOAD_STALL_TIMEOUT_MS = 15000;
    static constexpr unsigned long BOOT_STABILITY_MS = 10000;
    static constexpr unsigned long BOOT_VALIDATION_TIMEOUT_MS = 60000;
    static constexpr unsigned long RESTART_DELAY_MS = 1500;

    ConfigurationManager &configurationManager;
    WiFiClient networkClient;
    HTTPClient httpClient;
    OtaRecord record;
    FirmwareManifest activeManifest;
    std::string currentVersion;
    OtaRuntimeState runtimeState;
    size_t downloadedBytes;
    unsigned long lastDataAt;
    unsigned long stateStartedAt;

    bool LoadManifest(const std::string &url, FirmwareManifest &manifest,
                      OtaStartResult &failure);
    bool ValidateManifest(const FirmwareManifest &manifest, OtaStartResult &failure) const;
    bool BeginImageDownload(const FirmwareManifest &manifest);
    void DownloadNextChunk(unsigned long now);
    void FinishDownload(unsigned long now);
    void Fail(const char *failureCode);
    bool SaveRecord(const OtaRecord &candidate);
    void ValidateRunningImage(unsigned long now, bool localSystemHealthy);

    static bool IsAllowedUrl(const std::string &url);
    static bool IsLowerHex(const std::string &value, size_t requiredLength);
    static bool IsSemanticVersion(const std::string &value);
    static int CompareVersions(const std::string &left, const std::string &right);
};

