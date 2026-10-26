#include "FirmwareUpdateManager.h"

#include "OtaAuthentication.h"
#include "OtaManifestJson.h"
#include "../Constants/FirmwareVersion.h"
#include "../DeviceProvisioning/ProvisioningTypes.h"
#include "../Security/TrustedRootCa.h"

#include <Arduino.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_ota_ops.h>

#include <algorithm>
#include <array>

FirmwareUpdateManager::FirmwareUpdateManager(ConfigurationManager &manager)
    : configurationManager(manager), runtimeState(OtaRuntimeState::Idle), downloadedBytes(0),
      lastDataAt(0), stateStartedAt(0)
{
}

bool FirmwareUpdateManager::Begin(const std::string &runningVersion)
{
    if (!IsSemanticVersion(runningVersion))
        return false;
    currentVersion = runningVersion;

    OtaRecord loaded;
    const OtaLoadResult loadResult = configurationManager.LoadOta(loaded);
    if (loadResult == OtaLoadResult::Loaded)
    {
        record = std::move(loaded);
    }
    else if (loadResult == OtaLoadResult::NotFound)
    {
        if (!SaveRecord(OtaRecord{}))
            return false;
    }
    else
    {
        OtaRecord failed;
        failed.state = OtaPersistentState::Failed;
        failed.releaseId = "unknown-release";
        failed.targetVersion = runningVersion;
        failed.failureCode = "invalid_ota_record";
        if (!SaveRecord(failed))
            return false;
    }

    const esp_partition_t *runningPartition = esp_ota_get_running_partition();
    esp_ota_img_states_t imageState = ESP_OTA_IMG_UNDEFINED;
    const bool imageStateAvailable = runningPartition != nullptr &&
                                     esp_ota_get_state_partition(runningPartition, &imageState) == ESP_OK;
    const bool pendingBootValidation = imageStateAvailable &&
                                       (imageState == ESP_OTA_IMG_NEW ||
                                        imageState == ESP_OTA_IMG_PENDING_VERIFY);
    Serial.printf("Estado de imagen OTA del bootloader: %d\n", static_cast<int>(imageState));

    if (pendingBootValidation)
    {
        if (record.state != OtaPersistentState::PendingValidation ||
            record.targetVersion != currentVersion)
        {
            OtaRecord failed = record;
            if (failed.releaseId.empty()) failed.releaseId = "unknown-release";
            if (failed.targetVersion.empty()) failed.targetVersion = currentVersion;
            failed.state = OtaPersistentState::Failed;
            failed.failureCode = "version_mismatch";
            SaveRecord(failed);
            esp_ota_mark_app_invalid_rollback_and_reboot();
            return false;
        }
        runtimeState = OtaRuntimeState::PendingValidation;
        stateStartedAt = millis();
        return true;
    }

    if (record.state == OtaPersistentState::PendingValidation &&
        record.targetVersion == currentVersion)
    {
        OtaRecord confirmed = record;
        confirmed.state = OtaPersistentState::Confirmed;
        confirmed.failureCode.clear();
        if (!SaveRecord(confirmed))
            return false;
        runtimeState = OtaRuntimeState::Idle;
    }
    else if (record.state == OtaPersistentState::Downloading ||
             record.state == OtaPersistentState::PendingValidation)
    {
        OtaRecord failed = record;
        failed.state = OtaPersistentState::Failed;
        failed.failureCode = record.state == OtaPersistentState::Downloading
                                 ? "download_interrupted"
                                 : "rolled_back";
        if (!SaveRecord(failed))
            return false;
        runtimeState = OtaRuntimeState::Failed;
    }
    else
    {
        runtimeState = record.state == OtaPersistentState::Failed
                           ? OtaRuntimeState::Failed
                           : OtaRuntimeState::Idle;
    }
    return true;
}

OtaStartResult FirmwareUpdateManager::StartUpdate(const std::string &manifestUrl,
                                                   bool actuatorIsActive,
                                                   const std::string &expectedReleaseId,
                                                   const std::string &requestId,
                                                   uint32_t commandVersion)
{
    if (runtimeState == OtaRuntimeState::Downloading ||
        runtimeState == OtaRuntimeState::PendingValidation ||
        runtimeState == OtaRuntimeState::ReadyToRestart)
        return OtaStartResult::Busy;
    if (WiFi.status() != WL_CONNECTED)
        return OtaStartResult::WifiUnavailable;
    if (actuatorIsActive)
        return OtaStartResult::UnsafeActuatorState;
    if (!IsAllowedUrl(manifestUrl))
        return OtaStartResult::InvalidUrl;

    FirmwareManifest manifest;
    OtaStartResult failure = OtaStartResult::InvalidManifest;
    if (!LoadManifest(manifestUrl, manifest, failure) ||
        !ValidateManifest(manifest, failure))
        return failure;
    if (!expectedReleaseId.empty() && manifest.releaseId != expectedReleaseId)
        return OtaStartResult::ReleaseMismatch;

    OtaRecord downloading;
    downloading.state = OtaPersistentState::Downloading;
    downloading.releaseId = manifest.releaseId;
    downloading.targetVersion = manifest.version;
    downloading.requestId = requestId;
    downloading.commandVersion = commandVersion;
    if (!SaveRecord(downloading))
        return OtaStartResult::StorageError;

    if (!BeginImageDownload(manifest))
    {
        Fail("download_unavailable");
        return OtaStartResult::DownloadUnavailable;
    }

    activeManifest = std::move(manifest);
    downloadedBytes = 0;
    lastDataAt = millis();
    stateStartedAt = lastDataAt;
    runtimeState = OtaRuntimeState::Downloading;
    return OtaStartResult::Started;
}

void FirmwareUpdateManager::Loop(unsigned long now, bool localSystemHealthy)
{
    if (runtimeState == OtaRuntimeState::Downloading)
        DownloadNextChunk(now);
    else if (runtimeState == OtaRuntimeState::PendingValidation)
        ValidateRunningImage(now, localSystemHealthy);
    else if (runtimeState == OtaRuntimeState::ReadyToRestart &&
             static_cast<unsigned long>(now - stateStartedAt) >= RESTART_DELAY_MS)
        ESP.restart();
}

OtaRuntimeState FirmwareUpdateManager::GetState() const
{
    return runtimeState;
}

uint8_t FirmwareUpdateManager::GetProgressPercent() const
{
    if (activeManifest.size == 0)
        return 0;
    const size_t percentage = downloadedBytes * 100U / activeManifest.size;
    return static_cast<uint8_t>(percentage > 100 ? 100 : percentage);
}

const OtaRecord &FirmwareUpdateManager::GetRecord() const
{
    return record;
}

const std::string &FirmwareUpdateManager::GetCurrentVersion() const
{
    return currentVersion;
}

bool FirmwareUpdateManager::LoadManifest(const std::string &url, FirmwareManifest &manifest,
                                         OtaStartResult &failure)
{
    WiFiClient manifestNetworkClient;
    WiFiClientSecure secureManifestNetworkClient;
    // HTTPClient conserva una referencia al transporte; debe destruirse primero.
    HTTPClient manifestClient;
    manifestClient.setConnectTimeout(5000);
    manifestClient.setTimeout(5000);
    bool begun = false;
    if (url.rfind("https://", 0) == 0)
    {
#if ENABLE_LOCAL_OTA_MOCK
        if (HUERTA_TRUSTED_ROOT_CA[0] == '\0')
            secureManifestNetworkClient.setInsecure();
        else
            secureManifestNetworkClient.setCACert(HUERTA_TRUSTED_ROOT_CA);
#else
        if (HUERTA_TRUSTED_ROOT_CA[0] == '\0')
        {
            failure = OtaStartResult::InvalidUrl;
            return false;
        }
        secureManifestNetworkClient.setCACert(HUERTA_TRUSTED_ROOT_CA);
#endif
        begun = manifestClient.begin(secureManifestNetworkClient, url.c_str());
    }
    else
        begun = manifestClient.begin(manifestNetworkClient, url.c_str());
    if (!begun)
    {
        failure = OtaStartResult::ManifestUnavailable;
        return false;
    }
    const int status = manifestClient.GET();
    const int contentLength = manifestClient.getSize();
    if (status != HTTP_CODE_OK || contentLength <= 0 ||
        contentLength > static_cast<int>(MAX_MANIFEST_BYTES))
    {
        manifestClient.end();
        failure = OtaStartResult::ManifestUnavailable;
        return false;
    }
    const String body = manifestClient.getString();
    manifestClient.end();
    if (body.length() == 0 || body.length() > MAX_MANIFEST_BYTES ||
        !OtaManifestJson::Deserialize(std::string(body.c_str(), body.length()), manifest))
    {
        failure = OtaStartResult::InvalidManifest;
        return false;
    }
    return true;
}

bool FirmwareUpdateManager::ValidateManifest(const FirmwareManifest &manifest,
                                              OtaStartResult &failure) const
{
    if (manifest.contractVersion != FirmwareManifest::CONTRACT_VERSION ||
        !IsValidProvisioningIdentifier(manifest.releaseId, 64) ||
        !IsSemanticVersion(manifest.version) || manifest.board.empty() ||
        manifest.board.size() > 64 || manifest.size == 0 ||
        !IsLowerHex(manifest.sha256, 64) || !IsAllowedUrl(manifest.firmwareUrl) ||
        manifest.signatureHex.empty() || manifest.signatureHex.size() > 160)
    {
        failure = OtaStartResult::InvalidManifest;
        return false;
    }
    if (!OtaAuthentication::Verify(manifest))
    {
        failure = OtaStartResult::AuthenticationFailed;
        return false;
    }
    if (manifest.board != FIRMWARE_BOARD)
    {
        failure = OtaStartResult::IncompatibleBoard;
        return false;
    }
    if (CompareVersions(manifest.version, currentVersion) <= 0)
    {
        failure = OtaStartResult::VersionRejected;
        return false;
    }
    return true;
}

bool FirmwareUpdateManager::BeginImageDownload(const FirmwareManifest &manifest)
{
    httpClient.end();
    httpClient.setConnectTimeout(5000);
    httpClient.setTimeout(3000);
    bool begun = false;
    if (manifest.firmwareUrl.rfind("https://", 0) == 0)
    {
#if ENABLE_LOCAL_OTA_MOCK
        if (HUERTA_TRUSTED_ROOT_CA[0] == '\0')
            secureNetworkClient.setInsecure();
        else
            secureNetworkClient.setCACert(HUERTA_TRUSTED_ROOT_CA);
#else
        if (HUERTA_TRUSTED_ROOT_CA[0] == '\0')
            return false;
        secureNetworkClient.setCACert(HUERTA_TRUSTED_ROOT_CA);
#endif
        begun = httpClient.begin(secureNetworkClient, manifest.firmwareUrl.c_str());
    }
    else
        begun = httpClient.begin(networkClient, manifest.firmwareUrl.c_str());
    if (!begun)
        return false;
    const int status = httpClient.GET();
    if (status != HTTP_CODE_OK || httpClient.getSize() != static_cast<int>(manifest.size))
    {
        httpClient.end();
        return false;
    }
    if (!Update.begin(manifest.size, U_FLASH) || !Update.setSHA256(manifest.sha256.c_str()))
    {
        Update.abort();
        httpClient.end();
        return false;
    }
    return true;
}

void FirmwareUpdateManager::DownloadNextChunk(unsigned long now)
{
    NetworkClient *stream = httpClient.getStreamPtr();
    if (stream == nullptr)
    {
        Fail("stream_unavailable");
        return;
    }

    const int available = stream->available();
    if (available > 0)
    {
        std::array<uint8_t, 2048> buffer{};
        const size_t remaining = activeManifest.size - downloadedBytes;
        const size_t toRead = std::min<size_t>(buffer.size(),
                                               std::min<size_t>(available, remaining));
        const int read = stream->read(buffer.data(), toRead);
        if (read <= 0 || Update.write(buffer.data(), static_cast<size_t>(read)) !=
                             static_cast<size_t>(read))
        {
            Fail("flash_write_failed");
            return;
        }
        downloadedBytes += static_cast<size_t>(read);
        lastDataAt = now;
        if (downloadedBytes == activeManifest.size)
            FinishDownload(now);
        return;
    }

    if (static_cast<unsigned long>(now - lastDataAt) >= DOWNLOAD_STALL_TIMEOUT_MS ||
        (!httpClient.connected() && downloadedBytes < activeManifest.size))
        Fail("download_interrupted");
}

void FirmwareUpdateManager::FinishDownload(unsigned long now)
{
    if (!Update.end(false))
    {
        Fail(Update.errorString());
        return;
    }
    httpClient.end();

    OtaRecord pending = record;
    pending.state = OtaPersistentState::PendingValidation;
    pending.failureCode.clear();
    if (!SaveRecord(pending))
    {
        Fail("pending_state_not_saved");
        return;
    }
    runtimeState = OtaRuntimeState::ReadyToRestart;
    stateStartedAt = now;
    Serial.println("Firmware verificado. Reiniciando en la particion OTA...");
}

void FirmwareUpdateManager::Fail(const char *failureCode)
{
    if (Update.isRunning())
        Update.abort();
    httpClient.end();
    OtaRecord failed = record;
    if (failed.releaseId.empty()) failed.releaseId = "unknown-release";
    if (failed.targetVersion.empty()) failed.targetVersion = currentVersion;
    failed.state = OtaPersistentState::Failed;
    failed.failureCode = failureCode == nullptr ? "unknown" : failureCode;
    if (failed.failureCode.size() > 64)
        failed.failureCode.resize(64);
    SaveRecord(failed);
    runtimeState = OtaRuntimeState::Failed;
    Serial.printf("Actualizacion OTA fallida: %s\n", failed.failureCode.c_str());
}

bool FirmwareUpdateManager::SaveRecord(const OtaRecord &candidate)
{
    if (!configurationManager.SaveOta(candidate))
        return false;
    record = candidate;
    return true;
}

void FirmwareUpdateManager::ValidateRunningImage(unsigned long now, bool localSystemHealthy)
{
    const unsigned long elapsed = static_cast<unsigned long>(now - stateStartedAt);
    if (localSystemHealthy && elapsed >= BOOT_STABILITY_MS)
    {
        if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK)
        {
            Fail("boot_confirmation_failed");
            return;
        }
        OtaRecord confirmed = record;
        confirmed.state = OtaPersistentState::Confirmed;
        confirmed.failureCode.clear();
        if (!SaveRecord(confirmed))
        {
            Fail("confirmation_not_saved");
            return;
        }
        runtimeState = OtaRuntimeState::Idle;
        Serial.printf("Firmware OTA confirmado: %s\n", currentVersion.c_str());
        return;
    }
    if (elapsed >= BOOT_VALIDATION_TIMEOUT_MS)
    {
        OtaRecord failed = record;
        failed.state = OtaPersistentState::Failed;
        failed.failureCode = "boot_health_timeout";
        SaveRecord(failed);
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
}

bool FirmwareUpdateManager::IsAllowedUrl(const std::string &url)
{
#if ENABLE_LOCAL_OTA_MOCK
    return url.size() <= 512 &&
           (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0);
#else
    return url.size() <= 512 && url.rfind("https://", 0) == 0 &&
           HUERTA_TRUSTED_ROOT_CA[0] != '\0';
#endif
}

bool FirmwareUpdateManager::IsLowerHex(const std::string &value, size_t requiredLength)
{
    if (value.size() != requiredLength)
        return false;
    for (char character : value)
    {
        if (!((character >= '0' && character <= '9') ||
              (character >= 'a' && character <= 'f')))
            return false;
    }
    return true;
}

bool FirmwareUpdateManager::IsSemanticVersion(const std::string &value)
{
    if (value.empty() || value.size() > 32)
        return false;
    size_t dots = 0;
    bool digitInPart = false;
    for (char character : value)
    {
        if (character >= '0' && character <= '9')
        {
            digitInPart = true;
            continue;
        }
        if (character != '.' || !digitInPart || dots >= 2)
            return false;
        ++dots;
        digitInPart = false;
    }
    return dots == 2 && digitInPart;
}

int FirmwareUpdateManager::CompareVersions(const std::string &left, const std::string &right)
{
    const auto parse = [](const std::string &value)
    {
        std::array<uint32_t, 3> parts{};
        size_t part = 0;
        for (char character : value)
        {
            if (character == '.')
            {
                ++part;
                continue;
            }
            parts[part] = parts[part] * 10U + static_cast<uint32_t>(character - '0');
        }
        return parts;
    };
    const auto leftParts = parse(left);
    const auto rightParts = parse(right);
    for (size_t index = 0; index < leftParts.size(); ++index)
    {
        if (leftParts[index] < rightParts[index]) return -1;
        if (leftParts[index] > rightParts[index]) return 1;
    }
    return 0;
}

