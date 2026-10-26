#include "SerialProvisioningMock.h"

#include "ProvisioningJson.h"
#include "../CredentialProvisioning/CredentialProvisioningJson.h"
#include "../Recovery/RecoveryJson.h"
#include "../Ota/OtaControlJson.h"

#include <Arduino.h>
#include <ArduinoJson.h>

namespace
{
    bool TryParseWifiOutageDuration(const std::string &input, uint32_t &durationSeconds)
    {
        constexpr const char *PREFIX = "WIFI_OUTAGE ";
        constexpr size_t PREFIX_LENGTH = 12;
        constexpr uint32_t MAX_DURATION_SECONDS = 3600;

        if (input.rfind(PREFIX, 0) != 0 || input.size() == PREFIX_LENGTH)
            return false;

        uint32_t parsed = 0;
        for (size_t index = PREFIX_LENGTH; index < input.size(); ++index)
        {
            const char character = input[index];
            if (character < '0' || character > '9')
                return false;

            parsed = parsed * 10U + static_cast<uint32_t>(character - '0');
            if (parsed > MAX_DURATION_SECONDS)
                return false;
        }

        if (parsed == 0)
            return false;

        durationSeconds = parsed;
        return true;
    }
}

SerialProvisioningMock::SerialProvisioningMock(
    ProvisioningManager &wifiManager,
    DeviceProvisioningManager &provisioningManager,
    MqttCredentialProvisioningManager &mqttCredentialManager,
    DeviceRecoveryManager &deviceRecoveryManager,
    FirmwareUpdateManager &otaManager,
    Sensors &deviceSensors,
    ManagedProvisioningManager &webManagedProvisioningManager)
    : wifiProvisioningManager(wifiManager), manager(provisioningManager),
      credentialManager(mqttCredentialManager),
      recoveryManager(deviceRecoveryManager), firmwareUpdateManager(otaManager), sensors(deviceSensors),
      managedProvisioningManager(webManagedProvisioningManager)
{
    inputBuffer.reserve(MAX_RESPONSE_LENGTH);
}

void SerialProvisioningMock::AnnouncePendingCredentialRequest() const
{
    MqttCredentialRequest request;
    std::string key;
    if (!credentialManager.TryGetPendingRequest(request) ||
        !credentialManager.TryGetLocalBootstrapKey(key))
        return;

    Serial.println("MQTT_CREDENTIALS_LOCAL_KEY_BEGIN");
    Serial.println(key.c_str());
    Serial.println("MQTT_CREDENTIALS_LOCAL_KEY_END");
    Serial.println("MQTT_CREDENTIALS_REQUEST_BEGIN");
    Serial.println(CredentialProvisioningJson::Serialize(request).c_str());
    Serial.println("MQTT_CREDENTIALS_REQUEST_END");
}

void SerialProvisioningMock::AnnouncePendingRequest() const
{
    ClaimRequest request;
    if (!manager.TryGetPendingRequest(request))
        return;

    Serial.println("PROVISIONING_REQUEST_BEGIN");
    Serial.println(ProvisioningJson::Serialize(request).c_str());
    Serial.println("PROVISIONING_REQUEST_END");
}

void SerialProvisioningMock::SetDeviceInfo(const std::string &newDeviceId,
                                           const std::string &newMqttClientId,
                                           const std::string &newFirmwareVersion)
{
    deviceId = newDeviceId;
    mqttClientId = newMqttClientId;
    firmwareVersion = newFirmwareVersion;
}

void SerialProvisioningMock::Loop(unsigned long now)
{
    std::string event;
    while (managedProvisioningManager.TryTakeEvent(event))
        Serial.println(event.c_str());

    while (Serial.available() > 0)
    {
        const char character = static_cast<char>(Serial.read());
        // Algunos monitores envian CR, otros LF y PlatformIO puede enviar CRLF.
        // Procesar cualquiera de los dos permite usar todos sin dejar el comando
        // retenido indefinidamente; el segundo byte de CRLF encuentra el buffer vacio.
        if (character == '\r' || character == '\n')
        {
            if (!inputBuffer.empty())
                ProcessLine(now);
            inputBuffer.clear();
            continue;
        }

        if (inputBuffer.size() >= MAX_RESPONSE_LENGTH)
        {
            inputBuffer.clear();
            Serial.println("Respuesta de aprovisionamiento descartada: longitud excesiva");
            continue;
        }

        inputBuffer.push_back(character);
    }
}

void SerialProvisioningMock::ProcessLine(unsigned long now)
{
    if (inputBuffer.empty())
        return;

    std::string managedResponse;
    if (managedProvisioningManager.ProcessSerialMessage(inputBuffer, now, managedResponse))
    {
        if (!managedResponse.empty())
            Serial.println(managedResponse.c_str());
        return;
    }

#if ENABLE_LOCAL_PROVISIONING_MOCK
    if (inputBuffer == "PROVISIONING_REQUEST")
    {
        AnnouncePendingRequest();
        return;
    }

    if (inputBuffer == "DEVICE_INFO")
    {
        JsonDocument document;
        document["type"] = "device_info";
        document["deviceId"] = deviceId;
        document["mqttClientId"] = mqttClientId;
        document["firmwareVersion"] = firmwareVersion;
        Serial.println("DEVICE_INFO_BEGIN");
        serializeJson(document, Serial);
        Serial.println();
        Serial.println("DEVICE_INFO_END");
        return;
    }

    if (inputBuffer == "MQTT_CREDENTIALS_REQUEST")
    {
        AnnouncePendingCredentialRequest();
        return;
    }

    if (inputBuffer == "MQTT_CREDENTIALS_REQUEST_NEW")
    {
        Serial.println(credentialManager.StartRequest()
                           ? "Nueva solicitud de credenciales creada"
                           : "No fue posible crear otra solicitud de credenciales");
        AnnouncePendingCredentialRequest();
        return;
    }

    if (inputBuffer == "OTA_STATUS")
    {
        Serial.printf("Estado OTA: %s | Progreso: %u%%\n",
                      ToString(firmwareUpdateManager.GetState()),
                      firmwareUpdateManager.GetProgressPercent());
        return;
    }

    if (inputBuffer.rfind("WIFI_OUTAGE", 0) == 0)
    {
        uint32_t durationSeconds = 0;
        if (!TryParseWifiOutageDuration(inputBuffer, durationSeconds))
        {
            Serial.println("Uso: WIFI_OUTAGE <segundos>; rango permitido: 1-3600");
            return;
        }

        if (!wifiProvisioningManager.StartWifiOutageTest(now, durationSeconds))
            Serial.println("No fue posible iniciar WIFI_OUTAGE: prueba activa o Wi-Fi no configurado");
        return;
    }

    ClaimResponse response;
    if (ProvisioningJson::DeserializeResponse(inputBuffer, response))
    {
        const ClaimProcessingResult result = manager.ProcessResponse(response, now);
        Serial.printf("Resultado de vinculacion: %s\n", ToString(result));
        return;
    }

    EncryptedMqttCredentials envelope;
    if (CredentialProvisioningJson::DeserializeEnvelope(inputBuffer, envelope))
    {
        const CredentialEnvelopeResult result = credentialManager.ProcessEnvelope(envelope);
        Serial.printf("Resultado de credenciales MQTT: %s\n", ToString(result));
        return;
    }

    RecoveryCommand recoveryCommand;
    if (RecoveryJson::DeserializeCommand(inputBuffer, recoveryCommand))
    {
        std::string key;
        if (!credentialManager.TryGetLocalBootstrapKey(key))
        {
            Serial.println("Resultado de recuperacion: invalid_state");
            return;
        }
        const RecoveryCommandResult result = recoveryManager.ProcessCommand(recoveryCommand, key);
        Serial.printf("Resultado de recuperacion: %s\n", ToString(result));
        return;
    }

    std::string manifestUrl;
    if (OtaControlJson::TryParseUpdateRequest(inputBuffer, manifestUrl))
    {
        const OtaStartResult result = firmwareUpdateManager.StartUpdate(
            manifestUrl, sensors.IsWaterPumpActive());
        Serial.printf("Resultado de actualizacion OTA: %s\n", ToString(result));
        return;
    }

    Serial.println("Respuesta local descartada: JSON o contrato invalido");
#else
    Serial.println("Comando serie no admitido");
#endif
}

