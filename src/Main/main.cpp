#include <Arduino.h>
#include "../Sensors/Sensors.h"
#include "../Constants/Constants.h"
#include "../Comm/MqttCommunication.h"
#include "../Configuration/ConfigurationManager.h"
#include "../Provisioning/ProvisioningManager.h"
#include "../DeviceProvisioning/DeviceProvisioningManager.h"
#include "../CredentialProvisioning/MqttCredentialProvisioningManager.h"
#include "../Recovery/DeviceRecoveryManager.h"
#include "../Ota/FirmwareUpdateManager.h"
#include "../Identity/DeviceIdentity.h"
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
    DeviceIdentity hardwareIdentity;
    bool startupReady = false;

    constexpr unsigned long CREDENTIAL_CONNECTION_TIMEOUT_MS = 20000;
    constexpr unsigned long CREDENTIAL_STABILITY_WINDOW_MS = 5000;
    constexpr size_t SERIAL_RX_BUFFER_SIZE = 8192;

    DeviceConfiguration BuildFactoryConfiguration(const DeviceIdentity &identity)
    {
        DeviceConfiguration configuration;
        configuration.deviceId = identity.deviceId;
        configuration.mqttClientId = identity.mqttClientId;
        return configuration;
    }

    bool HasApprovedMqttCredentials()
    {
        return credentialProvisioningInitialized &&
               mqttCredentialManager.GetAppliedCredentialVersion() > 0 &&
               activeConfiguration.HasMqttConfiguration() &&
               !deviceRecoveryManager.IsMqttRevoked();
    }

    void TryStartMqtt()
    {
        if (mqttInitialized || credentialUpdateInProgress ||
            !deviceProvisioningManager.IsProvisioned() || !HasApprovedMqttCredentials())
            return;

        mqttInitialized = comm.Init(activeConfiguration);
        if (!mqttInitialized)
            Serial.println("MQTT deshabilitado: configuracion o credenciales no disponibles");
    }

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
        TryStartMqtt();
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
            return;
        }

        if (state == DeviceProvisioningState::Provisioned)
        {
            BeginCredentialProvisioning();
            TryStartMqtt();
        }
    }

    void TryStartClaimWhenOnline(unsigned long now)
    {
        if (deviceProvisioningManager.GetState() != DeviceProvisioningState::Unprovisioned ||
            provisioningManager.GetState() != ProvisioningState::Connected)
            return;

        if (!deviceProvisioningManager.StartClaim(now))
            Serial.println("No fue posible iniciar la solicitud de vinculacion");
    }

    bool ReconcileHardwareIdentity(DeviceConfiguration &configuration)
    {
        if (configuration.deviceId == hardwareIdentity.deviceId &&
            configuration.mqttClientId == hardwareIdentity.mqttClientId)
            return true;

        ProvisioningRecord provisioningRecord;
        const ProvisioningLoadResult provisioningResult =
            configurationManager.LoadProvisioning(provisioningRecord);
        if (provisioningResult == ProvisioningLoadResult::Loaded &&
            provisioningRecord.state == DeviceProvisioningState::Provisioned)
        {
            Serial.printf("Identidad vinculada conservada: %s | Hardware: %s\n",
                          configuration.deviceId.c_str(), hardwareIdentity.hardwareId.c_str());
            return true;
        }

        if (!configurationManager.ClearCredentialSecurity() ||
            !configurationManager.ClearRecovery() ||
            !configurationManager.ClearProvisioning())
            return false;

        DeviceConfiguration migrated = configuration;
        migrated.deviceId = hardwareIdentity.deviceId;
        migrated.mqttClientId = hardwareIdentity.mqttClientId;
        if (!configurationManager.Save(migrated))
            return false;

        configuration = std::move(migrated);
        Serial.printf("Identidad de hardware aplicada: %s | MQTT ClientId: %s\n",
                      configuration.deviceId.c_str(), configuration.mqttClientId.c_str());
        return true;
    }
}
void setup()
{
    // Los contratos locales cifrados pueden superar ampliamente los 256 bytes
    // del buffer USB CDC predeterminado. Configurarlo antes de begin() evita que
    // una pegada en bloque pierda el final del JSON y su terminador de linea.
    Serial.setRxBufferSize(SERIAL_RX_BUFFER_SIZE);
    Serial.begin(115200);
    if (!DeviceIdentity::TryCreate(hardwareIdentity))
    {
        Serial.println("No fue posible obtener la identidad unica desde eFuse");
        return;
    }
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
        if (!ReconcileHardwareIdentity(activeConfiguration))
        {
            configurationAvailable = false;
            Serial.println("No fue posible reconciliar la identidad de hardware");
        }
    }
    else
    {
        activeConfiguration = BuildFactoryConfiguration(hardwareIdentity);
        Serial.println("Configuracion NVS ausente o invalida. Creando estado de fabrica...");

        if (!configurationManager.Save(activeConfiguration))
        {
            Serial.println("Error guardando o verificando el estado de fabrica en NVS");
        }
        else
        {
            configurationAvailable = true;
            Serial.printf("Estado de fabrica listo | DeviceId: %s | MQTT ClientId: %s\n",
                          activeConfiguration.deviceId.c_str(),
                          activeConfiguration.mqttClientId.c_str());
        }
    }

#if ENABLE_LOCAL_PROVISIONING_MOCK
    if (configurationAvailable)
        serialProvisioningMock.SetDeviceInfo(activeConfiguration.deviceId,
                                             activeConfiguration.mqttClientId,
                                             FIRMWARE_VERSION);
#endif

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
    startupReady = true;
}

void loop()
{
    if (!startupReady)
    {
        delay(1000);
        return;
    }

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
        {
            BeginCredentialProvisioning();
            TryStartMqtt();
        }
    }

    TryStartClaimWhenOnline(currentTime);

    while (deviceProvisioningManager.TryTakeStateChange(state))
        ApplyProvisioningState(state);

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
