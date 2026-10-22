#include "OtaTypes.h"

#include "../DeviceProvisioning/ProvisioningTypes.h"

bool OtaRecord::IsValid() const
{
    if (recordVersion != CURRENT_VERSION || failureCode.size() > 64)
        return false;
    if (state == OtaPersistentState::Idle)
        return releaseId.empty() && targetVersion.empty() && failureCode.empty();
    return IsValidProvisioningIdentifier(releaseId, 64) &&
           !targetVersion.empty() && targetVersion.size() <= 32;
}

const char *ToString(OtaPersistentState state)
{
    switch (state)
    {
    case OtaPersistentState::Idle: return "idle";
    case OtaPersistentState::Downloading: return "downloading";
    case OtaPersistentState::PendingValidation: return "pending_validation";
    case OtaPersistentState::Confirmed: return "confirmed";
    case OtaPersistentState::Failed: return "failed";
    }
    return "unknown";
}

const char *ToString(OtaRuntimeState state)
{
    switch (state)
    {
    case OtaRuntimeState::Idle: return "idle";
    case OtaRuntimeState::Downloading: return "downloading";
    case OtaRuntimeState::PendingValidation: return "pending_validation";
    case OtaRuntimeState::ReadyToRestart: return "ready_to_restart";
    case OtaRuntimeState::Failed: return "failed";
    }
    return "unknown";
}

const char *ToString(OtaStartResult result)
{
    switch (result)
    {
    case OtaStartResult::Started: return "started";
    case OtaStartResult::Busy: return "busy";
    case OtaStartResult::WifiUnavailable: return "wifi_unavailable";
    case OtaStartResult::UnsafeActuatorState: return "unsafe_actuator_state";
    case OtaStartResult::InvalidUrl: return "invalid_url";
    case OtaStartResult::ManifestUnavailable: return "manifest_unavailable";
    case OtaStartResult::InvalidManifest: return "invalid_manifest";
    case OtaStartResult::AuthenticationFailed: return "authentication_failed";
    case OtaStartResult::IncompatibleBoard: return "incompatible_board";
    case OtaStartResult::VersionRejected: return "version_rejected";
    case OtaStartResult::StorageError: return "storage_error";
    case OtaStartResult::DownloadUnavailable: return "download_unavailable";
    case OtaStartResult::ImageRejected: return "image_rejected";
    }
    return "unknown";
}

