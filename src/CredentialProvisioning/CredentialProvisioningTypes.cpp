#include "CredentialProvisioningTypes.h"

namespace
{
    bool IsLowerHex(const std::string &value, size_t requiredLength)
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
}

bool CredentialSecurityRecord::IsValid() const
{
    return version == CURRENT_VERSION &&
           IsLowerHex(encryptionKeyHex, 64) &&
           (pendingRequestId.empty() || IsLowerHex(pendingRequestId, 32));
}

const char *ToString(CredentialEnvelopeResult result)
{
    switch (result)
    {
    case CredentialEnvelopeResult::Accepted:
        return "accepted";
    case CredentialEnvelopeResult::InvalidState:
        return "invalid_state";
    case CredentialEnvelopeResult::InvalidContract:
        return "invalid_contract";
    case CredentialEnvelopeResult::RequestMismatch:
        return "request_mismatch";
    case CredentialEnvelopeResult::AuthenticationFailed:
        return "authentication_failed";
    case CredentialEnvelopeResult::StaleVersion:
        return "stale_version";
    case CredentialEnvelopeResult::InvalidCredentials:
        return "invalid_credentials";
    case CredentialEnvelopeResult::StorageError:
        return "storage_error";
    }
    return "unknown";
}

