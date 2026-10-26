#pragma once

#include <DNSServer.h>
#include <WebServer.h>
#include <cstdint>
#include <string>

#include "../Configuration/ConfigurationManager.h"

enum class ProvisioningState
{
    Stopped,
    Connecting,
    Connected,
    PortalActive,
    TestingCredentials
};

class ProvisioningManager
{
public:
    explicit ProvisioningManager(ConfigurationManager &configurationManager);

    bool Begin(const DeviceConfiguration &configuration);
    void Loop();
    bool TryTakeUpdatedConfiguration(DeviceConfiguration &configuration);
    bool StartManagedWifiConfiguration(const std::string &ssid, const std::string &password);
    bool TryTakeCredentialTestResult(bool &success, std::string &detail);
    ProvisioningState GetState() const;
    bool StartWifiOutageTest(unsigned long now, uint32_t durationSeconds);

private:
    static constexpr unsigned long RECOVERY_DELAY_MS = 30000;
    static constexpr unsigned long CREDENTIAL_TEST_TIMEOUT_MS = 20000;
    static constexpr unsigned long PORTAL_ANNOUNCEMENT_INTERVAL_MS = 10000;

    ConfigurationManager &configurationManager;
    WebServer webServer;
    DNSServer dnsServer;
    DeviceConfiguration activeConfiguration;
    DeviceConfiguration candidateConfiguration;
    DeviceConfiguration updatedConfiguration;
    ProvisioningState state;
    unsigned long stateStartedAt;
    bool updatePending;
    bool credentialTestResultPending;
    bool credentialTestSucceeded;
    bool managedCredentialTest;
    bool routesConfigured;
    bool wifiOutageTestActive;
    unsigned long wifiOutageTestStartedAt;
    unsigned long wifiOutageTestDurationMs;
    unsigned long lastPortalAnnouncementAt;
    std::string accessPassword;
    std::string portalStatus;
    std::string credentialTestDetail;

    void StartStation(const std::string &ssid, const std::string &password);
    bool StartPortal();
    void StopPortal();
    void ConfigureRoutes();
    bool EnsureAuthenticated();
    void HandleRoot();
    void HandleConfigure();
    void HandleStatus();
    void HandleCaptivePortalProbe();
    void HandleNotFound();
    bool StartCredentialTest(const std::string &ssid, const std::string &password,
                             bool managedRequest);
    void CompleteCredentialTest();
    void FailCredentialTest(const char *message);
    void AnnouncePortalConnectionInfo();

    static std::string BuildAccessPassword();
    static std::string BuildAccessPointName(const std::string &deviceId);
    bool EnsureAccessPointPassword();
};
