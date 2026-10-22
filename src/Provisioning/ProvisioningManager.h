#pragma once

#include <DNSServer.h>
#include <WebServer.h>
#include <string>

#include "../Configuration/ConfigurationManager.h"

enum class ProvisioningState
{
    Stopped,
    Connecting,
    Connected,
    PortalActive,
    TestingCredentials,
    WindowExpired
};

class ProvisioningManager
{
public:
    explicit ProvisioningManager(ConfigurationManager &configurationManager);

    bool Begin(const DeviceConfiguration &configuration);
    void Loop();
    bool TryTakeUpdatedConfiguration(DeviceConfiguration &configuration);
    ProvisioningState GetState() const;

private:
    static constexpr unsigned long RECOVERY_DELAY_MS = 30000;
    static constexpr unsigned long CREDENTIAL_TEST_TIMEOUT_MS = 20000;
    static constexpr unsigned long PORTAL_WINDOW_MS = 300000;

    ConfigurationManager &configurationManager;
    WebServer webServer;
    DNSServer dnsServer;
    DeviceConfiguration activeConfiguration;
    DeviceConfiguration candidateConfiguration;
    DeviceConfiguration updatedConfiguration;
    ProvisioningState state;
    unsigned long stateStartedAt;
    unsigned long portalStartedAt;
    bool updatePending;
    bool routesConfigured;
    std::string accessPassword;
    std::string portalStatus;

    void StartStation(const std::string &ssid, const std::string &password);
    bool StartPortal();
    void StopPortal();
    void ConfigureRoutes();
    bool EnsureAuthenticated();
    void HandleRoot();
    void HandleConfigure();
    void HandleStatus();
    void HandleNotFound();
    void CompleteCredentialTest();
    void FailCredentialTest(const char *message);

    static std::string BuildAccessPassword();
    static std::string BuildAccessPointName(const std::string &deviceId);
};
