#include "ProvisioningManager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>

namespace
{
    constexpr uint16_t DNS_PORT = 53;
    constexpr const char *PORTAL_USER = "admin";
    const IPAddress ACCESS_POINT_IP(192, 168, 4, 1);
    const IPAddress ACCESS_POINT_GATEWAY(192, 168, 4, 1);
    const IPAddress ACCESS_POINT_SUBNET(255, 255, 255, 0);

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
      stateStartedAt(0), updatePending(false), routesConfigured(false),
      wifiOutageTestActive(false), wifiOutageTestStartedAt(0), wifiOutageTestDurationMs(0)
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
    if (wifiOutageTestActive &&
        HasElapsed(wifiOutageTestStartedAt, wifiOutageTestDurationMs))
    {
        wifiOutageTestActive = false;
        wifiOutageTestStartedAt = 0;
        wifiOutageTestDurationMs = 0;

        if (state == ProvisioningState::PortalActive ||
            state == ProvisioningState::TestingCredentials)
            StopPortal();

        WiFi.setAutoReconnect(true);
        state = ProvisioningState::Stopped;
        StartStation(activeConfiguration.wifiSsid, activeConfiguration.wifiPassword);
        state = ProvisioningState::Connecting;
        stateStartedAt = millis();
        Serial.println("WIFI_OUTAGE finalizado; restaurando la conexion Wi-Fi desde NVS");
    }

    // La prueba representa una caida real de la red sin alterar las credenciales.
    // Si el driver intenta recuperar la estacion antes de tiempo, se desconecta otra vez.
    if (wifiOutageTestActive && WiFi.status() == WL_CONNECTED)
        WiFi.disconnect(false, false);

    if (state == ProvisioningState::PortalActive || state == ProvisioningState::TestingCredentials)
    {
        dnsServer.processNextRequest();
        webServer.handleClient();

    }

    if (state == ProvisioningState::Connecting)
    {
        if (!wifiOutageTestActive && WiFi.status() == WL_CONNECTED)
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

bool ProvisioningManager::StartWifiOutageTest(unsigned long now, uint32_t durationSeconds)
{
    constexpr uint32_t MIN_DURATION_SECONDS = 1;
    constexpr uint32_t MAX_DURATION_SECONDS = 3600;

    if (wifiOutageTestActive || !activeConfiguration.HasWifiConfiguration() ||
        durationSeconds < MIN_DURATION_SECONDS || durationSeconds > MAX_DURATION_SECONDS)
        return false;

    wifiOutageTestActive = true;
    wifiOutageTestStartedAt = now;
    wifiOutageTestDurationMs = static_cast<unsigned long>(durationSeconds) * 1000UL;

    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
    state = ProvisioningState::Connecting;
    stateStartedAt = now;

    Serial.printf("WIFI_OUTAGE activo durante %lu segundos; NVS no sera modificada\n",
                  static_cast<unsigned long>(durationSeconds));
    if (durationSeconds <= RECOVERY_DELAY_MS / 1000UL)
        Serial.println("Aviso: usa mas de 30 segundos para alcanzar a abrir el portal Huerta-*");

    return true;
}

void ProvisioningManager::StartStation(const std::string &ssid, const std::string &password)
{
    WiFi.mode(state == ProvisioningState::TestingCredentials ? WIFI_AP_STA : WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
}

bool ProvisioningManager::StartPortal()
{
    // Una vez abierto el portal se detiene por completo la estación. ESP32 comparte
    // una sola radio: reintentar STA puede cambiar de canal y volver inestable el AP.
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_AP);
    if (!EnsureAccessPointPassword())
    {
        Serial.println("No fue posible cargar o guardar la clave estable del portal");
        state = ProvisioningState::Connecting;
        stateStartedAt = millis();
        return false;
    }

    const std::string accessPointName = BuildAccessPointName(activeConfiguration.deviceId);

    if (!WiFi.softAPConfig(ACCESS_POINT_IP, ACCESS_POINT_GATEWAY, ACCESS_POINT_SUBNET))
    {
        Serial.println("No fue posible configurar la red local en 192.168.4.1");
        state = ProvisioningState::Connecting;
        stateStartedAt = millis();
        return false;
    }

    if (!WiFi.softAP(accessPointName.c_str(), accessPassword.c_str(), 1, false, 2))
    {
        Serial.println("No se pudo iniciar el modo de configuracion local");
        state = ProvisioningState::Connecting;
        stateStartedAt = millis();
        return false;
    }

    ConfigureRoutes();
    webServer.begin();
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    portalStatus = "Esperando credenciales Wi-Fi.";
    stateStartedAt = millis();
    state = ProvisioningState::PortalActive;

    Serial.println("Modo de configuracion local activo hasta recuperar la conexion Wi-Fi");
    Serial.printf("Red: %s | URL: http://%s | Usuario: %s\n",
                  accessPointName.c_str(), WiFi.softAPIP().toString().c_str(), PORTAL_USER);
    Serial.printf("Clave estable de acceso: %s\n", accessPassword.c_str());
    return true;
}

void ProvisioningManager::StopPortal()
{
    dnsServer.stop();
    webServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
}

void ProvisioningManager::ConfigureRoutes()
{
    if (routesConfigured)
        return;

    webServer.on("/", HTTP_GET, [this]() { HandleRoot(); });
    webServer.on("/configure", HTTP_POST, [this]() { HandleConfigure(); });
    webServer.on("/status", HTTP_GET, [this]() { HandleStatus(); });
    webServer.on("/generate_204", HTTP_GET, [this]() { HandleCaptivePortalProbe(); });
    webServer.on("/gen_204", HTTP_GET, [this]() { HandleCaptivePortalProbe(); });
    webServer.on("/hotspot-detect.html", HTTP_GET, [this]() { HandleCaptivePortalProbe(); });
    webServer.on("/library/test/success.html", HTTP_GET, [this]() { HandleCaptivePortalProbe(); });
    webServer.on("/connecttest.txt", HTTP_GET, [this]() { HandleCaptivePortalProbe(); });
    webServer.on("/ncsi.txt", HTTP_GET, [this]() { HandleCaptivePortalProbe(); });
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

    // En una prueba WIFI_OUTAGE, el envío del formulario representa la acción
    // explícita del usuario que autoriza un nuevo intento de estación.
    if (wifiOutageTestActive)
    {
        wifiOutageTestActive = false;
        wifiOutageTestStartedAt = 0;
        wifiOutageTestDurationMs = 0;
        Serial.println("WIFI_OUTAGE cancelado para probar las credenciales enviadas");
    }

    portalStatus = "Probando conexion. La configuracion aun no ha sido guardada.";
    state = ProvisioningState::TestingCredentials;
    stateStartedAt = millis();

    webServer.send(202, "text/html; charset=utf-8",
                   "<!doctype html><html lang=\"es\"><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width\"><p>Probando conexion...</p><p>La red Huerta puede desconectarse brevemente durante la prueba.</p><p><a href=\"/status\">Consultar estado</a></p></html>");

    // Se inicia STA solamente después de responder al navegador. Durante la prueba
    // el AP permanece levantado en modo AP+STA.
    StartStation(candidateConfiguration.wifiSsid, candidateConfiguration.wifiPassword);
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
    HandleCaptivePortalProbe();
}

void ProvisioningManager::HandleCaptivePortalProbe()
{
    webServer.sendHeader("Cache-Control", "no-store");
    webServer.sendHeader("Location", "http://192.168.4.1/", true);
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
    WiFi.setAutoReconnect(true);
    StopPortal();
    state = ProvisioningState::Connected;
}

void ProvisioningManager::FailCredentialTest(const char *message)
{
    portalStatus = message;
    state = ProvisioningState::PortalActive;
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_AP);
    Serial.println("Prueba Wi-Fi fallida; el portal permanece activo sin nuevos reintentos");
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

bool ProvisioningManager::EnsureAccessPointPassword()
{
    if (accessPassword.size() >= 8 && accessPassword.size() <= 63)
        return true;

    if (configurationManager.LoadAccessPointPassword(accessPassword))
        return true;

    const std::string generated = BuildAccessPassword();
    if (!configurationManager.SaveAccessPointPassword(generated))
        return false;

    accessPassword = generated;
    Serial.println("Clave estable del portal creada y guardada en NVS");
    return true;
}

std::string ProvisioningManager::BuildAccessPointName(const std::string &deviceId)
{
    constexpr size_t DISPLAY_SUFFIX_LENGTH = 6;
    std::string suffix;
    suffix.reserve(DISPLAY_SUFFIX_LENGTH);

    for (auto iterator = deviceId.rbegin();
         iterator != deviceId.rend() && suffix.size() < DISPLAY_SUFFIX_LENGTH; ++iterator)
    {
        char value = *iterator;
        const bool isDigit = value >= '0' && value <= '9';
        const bool isLower = value >= 'a' && value <= 'z';
        const bool isUpper = value >= 'A' && value <= 'Z';
        if (!isDigit && !isLower && !isUpper)
            continue;
        if (isLower)
            value = static_cast<char>(value - 'a' + 'A');
        suffix.insert(suffix.begin(), value);
    }

    return "Huerta-" + (suffix.empty() ? std::string("Device") : suffix);
}
