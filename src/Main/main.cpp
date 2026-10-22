#include <Arduino.h>
#include "../Sensors/Sensors.h"
#include "../Constants/Constants.h"
#include "../Comm/MqttCommunication.h"
#if ENABLE_DEVELOPMENT_BOOTSTRAP
#include "../Secrets.h"
#endif
#include "../Configuration/ConfigurationManager.h"
#include "../Provisioning/ProvisioningManager.h"
#include "../DeviceProvisioning/DeviceProvisioningManager.h"
#include "../CredentialProvisioning/MqttCredentialProvisioningManager.h"
#include "../Recovery/DeviceRecoveryManager.h"
#include "../Ota/FirmwareUpdateManager.h"
#include "../Constants/FirmwareVersion.h"
#if ENABLE_LOCAL_PROVISIONING_MOCK
#include "../DeviceProvisioning/SerialProvisioningMock.h"
#endif
#include "Objects/FullTelemetry.h"
#include "ActionDispatcher/ActionDispatcher.h"

// Arduino-ESP32 confirma OTA antes de setup() por defecto. Se aplaza para que
// FirmwareUpdateManager valide NVS, sensores y estabilidad del arranque.
extern "C" bool verifyRollbackLater()
{
    return true;
}

namespace
{
    Sensors sensors = Sensors();
    ActionDispatcher dispatcher(sensors);
    ConfigurationManager configurationManager;
    ProvisioningManager provisioningManager(configurationManager);
    DeviceProvisioningManager deviceProvisioningManager(configurationManager);
    MqttCredentialProvisioningManager mqttCredentialManager(configurationManager);
    DeviceRecoveryManager deviceRecoveryManager(configurationManager, deviceProvisioningManager);
    FirmwareUpdateManager firmwareUpdateManager(configurationManager);
#if ENABLE_LOCAL_PROVISIONING_MOCK
    SerialProvisioningMock serialProvisioningMock(
        provisioningManager, deviceProvisioningManager, mqttCredentialManager, deviceRecoveryManager,
        firmwareUpdateManager, sensors);
#endif
    MqttCommunication comm;
    DeviceConfiguration activeConfiguration;
    bool mqttInitialized = false;
    bool credentialProvisioningInitialized = false;
    bool recoveryInitialized = false;
    bool localSystemHealthy = false;
    bool credentialUpdateInProgress = false;
    DeviceConfiguration credentialCandidate;
    uint32_t credentialCandidateVersion = 0;
    unsigned long credentialUpdateStartedAt = 0;
    unsigned long credentialConnectionStableStartedAt = 0;
    unsigned long lastRead = 0;

    constexpr unsigned long CREDENTIAL_CONNECTION_TIMEOUT_MS = 20000;
    constexpr unsigned long CREDENTIAL_STABILITY_WINDOW_MS = 5000;

    void BeginCredentialProvisioning()
    {
        if (credentialProvisioningInitialized)
            return;

        credentialProvisioningInitialized = mqttCredentialManager.Begin(
            activeConfiguration.deviceId, activeConfiguration,
            deviceProvisioningManager.IsProvisioned());
        if (!credentialProvisioningInitialized)
        {
            Serial.println("Aprovisionamiento de credenciales MQTT no disponible");
            return;
        }

        Serial.printf("Version de credenciales MQTT aplicada: %lu\n",
                      static_cast<unsigned long>(mqttCredentialManager.GetAppliedCredentialVersion()));
        if (deviceRecoveryManager.IsMqttRevoked())
            mqttCredentialManager.StartRequest();
#if ENABLE_LOCAL_PROVISIONING_MOCK
        serialProvisioningMock.AnnouncePendingCredentialRequest();
#endif
    }

    void RollBackCredentialUpdate(const char *reason)
    {
        Serial.printf("Revirtiendo credenciales MQTT: %s\n", reason);
        mqttCredentialManager.RejectCandidate();
        credentialUpdateInProgress = false;
        credentialCandidate = DeviceConfiguration{};
        credentialCandidateVersion = 0;
        credentialConnectionStableStartedAt = 0;
        mqttInitialized = comm.Init(activeConfiguration);
    }

    void ApplyProvisioningState(DeviceProvisioningState state)
    {
        Serial.printf("Estado de vinculacion: %s\n", ToString(state));

#if ENABLE_LOCAL_PROVISIONING_MOCK
        if (state == DeviceProvisioningState::PendingClaim)
            serialProvisioningMock.AnnouncePendingRequest();
#endif

        if (state == DeviceProvisioningState::Unprovisioned)
        {
            comm.Stop();
            mqttInitialized = false;
            credentialProvisioningInitialized = false;
            credentialUpdateInProgress = false;
            mqttCredentialManager.Reset();
            if (!deviceProvisioningManager.StartClaim(millis()))
                Serial.println("No fue posible iniciar una nueva solicitud de vinculacion");
            return;
        }

        if (state == DeviceProvisioningState::Provisioned &&
            !deviceRecoveryManager.IsMqttRevoked() &&
            !mqttInitialized && activeConfiguration.IsValid())
        {
            mqttInitialized = comm.Init(activeConfiguration);
            if (!mqttInitialized)
                Serial.println("MQTT deshabilitado: configuracion incompleta");
        }

        if (state == DeviceProvisioningState::Provisioned)
            BeginCredentialProvisioning();
    }

#if ENABLE_DEVELOPMENT_BOOTSTRAP
    DeviceConfiguration BuildDevelopmentConfiguration()
    {
        DeviceConfiguration configuration;
        configuration.deviceId = DEVICE_ID;
        configuration.wifiSsid = WIFI_SSID;
        configuration.wifiPassword = WIFI_PASSWORD;
        configuration.mqttHost = MQTT_HOST;
        configuration.mqttPort = MQTT_PORT;
        configuration.mqttClientId = MQTT_CLIENT_ID;
        configuration.mqttUser = MQTT_USER;
        configuration.mqttPassword = MQTT_PASSWORD;
        return configuration;
    }
#endif
}
void setup()
{
    Serial.begin(115200);
    if (!firmwareUpdateManager.Begin(FIRMWARE_VERSION))
        Serial.println("Gestor OTA no disponible");
    else
        Serial.printf("Firmware actual: %s | Estado OTA: %s\n", FIRMWARE_VERSION,
                      ToString(firmwareUpdateManager.GetState()));

    bool recoveredFromBackup = false;
    const ConfigurationLoadResult loadResult = configurationManager.LoadRecoverable(
        activeConfiguration, recoveredFromBackup);
    bool configurationAvailable = loadResult == ConfigurationLoadResult::Loaded;

    if (configurationAvailable)
    {
        Serial.printf("Configuracion cargada desde NVS | Version: %u | DeviceId: %s\n",
                      activeConfiguration.version, activeConfiguration.deviceId.c_str());
        if (recoveredFromBackup)
            Serial.println("Configuracion principal recuperada desde la ultima copia valida");
    }
    else
    {
#if ENABLE_DEVELOPMENT_BOOTSTRAP
        activeConfiguration = BuildDevelopmentConfiguration();
        Serial.println("Configuracion NVS ausente o invalida. Iniciando migracion de desarrollo...");

        if (!configurationManager.Save(activeConfiguration))
        {
            Serial.println("Error guardando o verificando la configuracion en NVS");
        }
        else
        {
            configurationAvailable = true;
            Serial.println("Configuracion migrada y verificada mediante lectura desde NVS");
        }
#else
        Serial.println("Configuracion NVS ausente o invalida. Comunicacion deshabilitada");
#endif
    }

    sensors.InitializeSensors();

    if (!configurationAvailable || !provisioningManager.Begin(activeConfiguration))
        Serial.println("Wi-Fi y aprovisionamiento local deshabilitados: falta configuracion base");

    bool deviceProvisioningAvailable = false;
    if (configurationAvailable &&
        deviceProvisioningManager.Begin(activeConfiguration.deviceId, millis()))
    {
        deviceProvisioningAvailable = true;
        recoveryInitialized = deviceRecoveryManager.Begin(activeConfiguration.deviceId);
        if (!recoveryInitialized)
            Serial.println("Recuperacion deshabilitada: no fue posible validar su estado persistente");

        if (deviceProvisioningManager.GetState() == DeviceProvisioningState::Unprovisioned &&
            !deviceProvisioningManager.StartClaim(millis()))
            Serial.println("No fue posible iniciar la solicitud de vinculacion");

        DeviceProvisioningState state;
        while (deviceProvisioningManager.TryTakeStateChange(state))
            ApplyProvisioningState(state);
    }
    else
    {
        Serial.println("Vinculacion deshabilitada: identidad no disponible");
    }

    Serial.print("Sensores inicializados...\n");
    Serial.print("LECTURAS OBTENIDAS \n");
    lastRead = millis();
    localSystemHealthy = configurationAvailable && deviceProvisioningAvailable;
}

void loop()
{
    provisioningManager.Loop();

    const unsigned long currentTime = millis();
    firmwareUpdateManager.Loop(currentTime, localSystemHealthy);
    deviceProvisioningManager.Loop(currentTime);
#if ENABLE_LOCAL_PROVISIONING_MOCK
    serialProvisioningMock.Loop(currentTime);
#endif

    if (recoveryInitialized && deviceRecoveryManager.IsMqttRevoked() && mqttInitialized &&
        !credentialUpdateInProgress)
    {
        comm.Stop();
        mqttInitialized = false;
        Serial.println("MQTT detenido: credenciales revocadas");
    }

    if (recoveryInitialized && deviceRecoveryManager.IsMqttRevoked() &&
        deviceProvisioningManager.IsProvisioned() && credentialProvisioningInitialized)
    {
        MqttCredentialRequest pendingRequest;
        if (!mqttCredentialManager.TryGetPendingRequest(pendingRequest) &&
            mqttCredentialManager.StartRequest())
        {
#if ENABLE_LOCAL_PROVISIONING_MOCK
            serialProvisioningMock.AnnouncePendingCredentialRequest();
#endif
        }
    }

    if (!credentialUpdateInProgress &&
        mqttCredentialManager.TryTakeCandidate(credentialCandidate, credentialCandidateVersion))
    {
        Serial.printf("Probando credenciales MQTT version %lu\n",
                      static_cast<unsigned long>(credentialCandidateVersion));
        credentialUpdateInProgress = true;
        credentialUpdateStartedAt = currentTime;
        credentialConnectionStableStartedAt = 0;
        mqttInitialized = comm.Init(credentialCandidate);
        if (!mqttInitialized)
            RollBackCredentialUpdate("configuracion rechazada por el cliente MQTT");
    }

    DeviceProvisioningState state;
    while (deviceProvisioningManager.TryTakeStateChange(state))
        ApplyProvisioningState(state);

    DeviceConfiguration updatedConfiguration;
    if (provisioningManager.TryTakeUpdatedConfiguration(updatedConfiguration))
    {
        activeConfiguration = updatedConfiguration;
        mqttCredentialManager.UpdateBaseConfiguration(activeConfiguration);
        if (deviceProvisioningManager.IsProvisioned())
            mqttInitialized = comm.Init(activeConfiguration);
    }

    if (mqttInitialized)
        comm.Loop();

    if (credentialUpdateInProgress)
    {
        if (comm.IsConnected())
        {
            if (credentialConnectionStableStartedAt == 0)
                credentialConnectionStableStartedAt = currentTime;

            if (static_cast<unsigned long>(currentTime - credentialConnectionStableStartedAt) >=
                CREDENTIAL_STABILITY_WINDOW_MS)
            {
                const DeviceConfiguration previousConfiguration = activeConfiguration;
                if (configurationManager.Save(credentialCandidate) &&
                    mqttCredentialManager.ConfirmApplied(credentialCandidateVersion))
                {
                    activeConfiguration = credentialCandidate;
                    credentialUpdateInProgress = false;
                    credentialCandidate = DeviceConfiguration{};
                    credentialConnectionStableStartedAt = 0;
                    if (deviceRecoveryManager.MarkCredentialsRestored())
                    {
                        Serial.printf("Credenciales MQTT version %lu verificadas y guardadas\n",
                                      static_cast<unsigned long>(credentialCandidateVersion));
                    }
                    else
                    {
                        comm.Stop();
                        mqttInitialized = false;
                        Serial.println("Credenciales guardadas, pero MQTT permanece bloqueado por seguridad");
                    }
                    credentialCandidateVersion = 0;
                }
                else
                {
                    configurationManager.Save(previousConfiguration);
                    RollBackCredentialUpdate("no fue posible confirmar la persistencia");
                }
            }
        }
        else
        {
            credentialConnectionStableStartedAt = 0;
            if (static_cast<unsigned long>(currentTime - credentialUpdateStartedAt) >=
                CREDENTIAL_CONNECTION_TIMEOUT_MS)
                RollBackCredentialUpdate("RabbitMQ rechazo las credenciales o no respondio");
        }
    }

    ActionMessage actionMessage{};
    if (firmwareUpdateManager.GetState() != OtaRuntimeState::Downloading &&
        comm.TryTakeAction(actionMessage))
    {
        ActionResult result = dispatcher.Dispatch(actionMessage);
        Serial.printf("Comando ejecutado: %s\n", result.success ? "SI" : "NO");
        comm.PublishActionResponse(result);
    }

    if (currentTime - lastRead >= READ_INTERVAL)
    {
        lastRead = currentTime;
        FullTelemetry fullTelemetry = sensors.GetDataFromSensors(true);
        comm.PublishTelemetry(fullTelemetry);
    }
}
