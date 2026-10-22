#include "ProvisioningManager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>

namespace
{
    constexpr uint16_t DNS_PORT = 53;
    constexpr const char *PORTAL_USER = "admin";

    const char PORTAL_PAGE[] = R"HTML(
<!doctype html><html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Configurar huerta</title>
<style>body{font:16px system-ui;max-width:30rem;margin:3rem auto;padding:0 1rem}label{display:block;margin-top:1rem}input,button{box-sizing:border-box;width:100%;padding:.75rem;margin-top:.35rem}small{color:#555}</style>
</head><body><h1>Configurar Wi-Fi</h1>
<p>Las credenciales solo se guardan si el dispositivo logra conectarse.</p>
<form method="post" action="/configure">
<label>SSID<input name="ssid" maxlength="32" required autocomplete="off"></label>
<label>Contraseña<input name="password" type="password" maxlength="64" autocomplete="new-password"></label>
<small>Dejar vacía únicamente para una red abierta.</small>
<button type="submit">Probar y guardar</button></form></body></html>)HTML";

    bool HasElapsed(unsigned long startedAt, unsigned long duration)
    {
        return static_cast<unsigned long>(millis() - startedAt) >= duration;
    }

    void OnWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
    {
        if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
        {
            Serial.print("Desconexion WiFi | Razon: ");
            Serial.println(info.wifi_sta_disconnected.reason);
        }
        else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP)
        {
            Serial.print("Conectado | IP: ");
            Serial.println(WiFi.localIP());
        }
    }
}

ProvisioningManager::ProvisioningManager(ConfigurationManager &manager)
    : configurationManager(manager), webServer(80), state(ProvisioningState::Stopped),
      stateStartedAt(0), portalStartedAt(0), updatePending(false), routesConfigured(false)
{
}

bool ProvisioningManager::Begin(const DeviceConfiguration &configuration)
{
    if (!configuration.IsPersistable())
        return false;

    activeConfiguration = configuration;
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.onEvent(OnWiFiEvent);

    if (activeConfiguration.HasWifiConfiguration())
    {
        StartStation(activeConfiguration.wifiSsid, activeConfiguration.wifiPassword);
        state = ProvisioningState::Connecting;
        stateStartedAt = millis();
        return true;
    }

    return StartPortal();
}

void ProvisioningManager::Loop()
{
    if (state == ProvisioningState::PortalActive || state == ProvisioningState::TestingCredentials)
    {
        dnsServer.processNextRequest();
        webServer.handleClient();

        if (HasElapsed(portalStartedAt, PORTAL_WINDOW_MS))
        {
            Serial.println("Ventana de configuracion finalizada");
            StopPortal();
            state = ProvisioningState::WindowExpired;

            if (activeConfiguration.HasWifiConfiguration())
                StartStation(activeConfiguration.wifiSsid, activeConfiguration.wifiPassword);
            return;
        }

        if (state == ProvisioningState::PortalActive &&
            activeConfiguration.HasWifiConfiguration() &&
            WiFi.status() == WL_CONNECTED)
        {
            Serial.println("Wi-Fi recuperado; cerrando configuracion local");
            StopPortal();
            state = ProvisioningState::Connected;
            return;
        }
    }

    if (state == ProvisioningState::Connecting)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            state = ProvisioningState::Connected;
        }
        else if (HasElapsed(stateStartedAt, RECOVERY_DELAY_MS))
        {
            StartPortal();
        }
    }
    else if (state == ProvisioningState::Connected && WiFi.status() != WL_CONNECTED)
    {
        state = ProvisioningState::Connecting;
        stateStartedAt = millis();
    }
    else if (state == ProvisioningState::TestingCredentials)
    {
        if (WiFi.status() == WL_CONNECTED)
            CompleteCredentialTest();
        else if (HasElapsed(stateStartedAt, CREDENTIAL_TEST_TIMEOUT_MS))
            FailCredentialTest("No fue posible conectar. Revisa SSID y contrasena.");
    }
}

bool ProvisioningManager::TryTakeUpdatedConfiguration(DeviceConfiguration &configuration)
{
    if (!updatePending)
        return false;

    configuration = updatedConfiguration;
    updatePending = false;
    return true;
}

ProvisioningState ProvisioningManager::GetState() const
{
    return state;
}

void ProvisioningManager::StartStation(const std::string &ssid, const std::string &password)
{
    WiFi.mode(state == ProvisioningState::PortalActive || state == ProvisioningState::TestingCredentials
                  ? WIFI_AP_STA
                  : WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
}

bool ProvisioningManager::StartPortal()
{
    WiFi.mode(WIFI_AP_STA);
    accessPassword = BuildAccessPassword();
    const std::string accessPointName = BuildAccessPointName(activeConfiguration.deviceId);

    if (!WiFi.softAP(accessPointName.c_str(), accessPassword.c_str(), 1, false, 1))
    {
        Serial.println("No se pudo iniciar el modo de configuracion local");
        state = ProvisioningState::Stopped;
        return false;
    }

    ConfigureRoutes();
    webServer.begin();
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    portalStatus = "Esperando credenciales Wi-Fi.";
    portalStartedAt = millis();
    stateStartedAt = portalStartedAt;
    state = ProvisioningState::PortalActive;

    Serial.println("Modo de configuracion local activo durante 5 minutos");
    Serial.printf("Red: %s | URL: http://%s | Usuario: %s\n",
                  accessPointName.c_str(), WiFi.softAPIP().toString().c_str(), PORTAL_USER);
    Serial.printf("Clave temporal de acceso: %s\n", accessPassword.c_str());
    return true;
}

void ProvisioningManager::StopPortal()
{
    dnsServer.stop();
    webServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    accessPassword.clear();
}

void ProvisioningManager::ConfigureRoutes()
{
    if (routesConfigured)
        return;

    webServer.on("/", HTTP_GET, [this]() { HandleRoot(); });
    webServer.on("/configure", HTTP_POST, [this]() { HandleConfigure(); });
    webServer.on("/status", HTTP_GET, [this]() { HandleStatus(); });
    webServer.onNotFound([this]() { HandleNotFound(); });
    routesConfigured = true;
}

bool ProvisioningManager::EnsureAuthenticated()
{
    if (webServer.authenticate(PORTAL_USER, accessPassword.c_str()))
        return true;

    webServer.requestAuthentication(BASIC_AUTH, "Huerta - configuracion local");
    return false;
}

void ProvisioningManager::HandleRoot()
{
    if (!EnsureAuthenticated())
        return;

    webServer.sendHeader("Cache-Control", "no-store");
    webServer.send(200, "text/html; charset=utf-8", PORTAL_PAGE);
}

void ProvisioningManager::HandleConfigure()
{
    if (!EnsureAuthenticated())
        return;

    const String ssid = webServer.arg("ssid");
    const String password = webServer.arg("password");

    if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 64 ||
        (!password.isEmpty() && password.length() < 8))
    {
        webServer.send(400, "text/plain; charset=utf-8", "SSID o contrasena no validos");
        return;
    }

    candidateConfiguration = activeConfiguration;
    candidateConfiguration.wifiSsid.assign(ssid.c_str(), ssid.length());
    candidateConfiguration.wifiPassword.assign(password.c_str(), password.length());

    portalStatus = "Probando conexion. La configuracion aun no ha sido guardada.";
    state = ProvisioningState::TestingCredentials;
    stateStartedAt = millis();
    StartStation(candidateConfiguration.wifiSsid, candidateConfiguration.wifiPassword);

    webServer.send(202, "text/html; charset=utf-8",
                   "<!doctype html><html lang=\"es\"><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width\"><p>Probando conexion...</p><p><a href=\"/status\">Consultar estado</a></p></html>");
}

void ProvisioningManager::HandleStatus()
{
    if (!EnsureAuthenticated())
        return;

    webServer.sendHeader("Cache-Control", "no-store");
    webServer.send(200, "text/plain; charset=utf-8", portalStatus.c_str());
}

void ProvisioningManager::HandleNotFound()
{
    if (!EnsureAuthenticated())
        return;

    webServer.sendHeader("Location", "/", true);
    webServer.send(302, "text/plain", "");
}

void ProvisioningManager::CompleteCredentialTest()
{
    if (!configurationManager.Save(candidateConfiguration))
    {
        FailCredentialTest("La red funciona, pero no fue posible guardar la configuracion.");
        return;
    }

    activeConfiguration = candidateConfiguration;
    updatedConfiguration = activeConfiguration;
    updatePending = true;
    portalStatus = "Conexion verificada y configuracion guardada.";
    Serial.println("Credenciales Wi-Fi verificadas y guardadas en NVS");
    StopPortal();
    state = ProvisioningState::Connected;
}

void ProvisioningManager::FailCredentialTest(const char *message)
{
    portalStatus = message;
    state = ProvisioningState::PortalActive;

    if (activeConfiguration.HasWifiConfiguration())
        StartStation(activeConfiguration.wifiSsid, activeConfiguration.wifiPassword);
}

std::string ProvisioningManager::BuildAccessPassword()
{
    static constexpr char ALPHABET[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
    static constexpr size_t PASSWORD_LENGTH = 12;
    std::string password;
    password.reserve(PASSWORD_LENGTH);

    for (size_t i = 0; i < PASSWORD_LENGTH; ++i)
        password.push_back(ALPHABET[esp_random() % (sizeof(ALPHABET) - 1)]);

    return password;
}

std::string ProvisioningManager::BuildAccessPointName(const std::string &deviceId)
{
    std::string suffix;
    suffix.reserve(16);

    for (char value : deviceId)
    {
        const bool valid = (value >= 'a' && value <= 'z') ||
                           (value >= 'A' && value <= 'Z') ||
                           (value >= '0' && value <= '9') || value == '-';
        if (valid)
            suffix.push_back(value);
        if (suffix.size() == 16)
            break;
    }

    return "Huerta-" + (suffix.empty() ? std::string("Device") : suffix);
}
