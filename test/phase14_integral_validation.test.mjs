import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { buildEvidence, validateWebEvidence } from '../tools/phase14_evidence.mjs';

function webEvidence(deviceId, hardwareId) {
  return {
    schemaVersion: 1,
    events: [
      { payload: { type: 'device_identify_result', success: true, deviceId, hardwareId,
        firmwareVersion: '1.8.0', board: 'esp32-c3-devkitm-1' } },
      { payload: { type: 'device_configuration_result', success: true, deviceId,
        provisioningState: 'provisioned', wifiConnected: true, mqttConnected: true,
        mqttCredentialVersion: 1 } },
    ],
  };
}

function mqttRecords(deviceId) {
  return [
    { topic: `gardens/${deviceId}/inventory`, payload: {
      type: 'device_inventory', deviceId, devices: Array.from({ length: 6 }, (_, id) => ({ id })) } },
    { topic: `gardens/${deviceId}/telemetry`, payload: { readings: [] } },
    { topic: `gardens/${deviceId}/actions-response`, payload: {
      type: 'device_action_result', deviceId, requestId: `${deviceId}-action` } },
    { topic: `gardens/${deviceId}/control-status`, payload: {
      type: 'ota_update_status', deviceId, status: 'Confirmed',
      requestId: `${deviceId}-ota`, releaseId: 'release-1.8.0' } },
  ];
}

test('aprueba dos dispositivos distintos con ciclo MQTT completo', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'huerta-phase14-'));
  try {
    const developmentBin = path.join(directory, 'development.bin');
    const productionBin = path.join(directory, 'production.bin');
    fs.writeFileSync(developmentBin, 'development-firmware');
    fs.writeFileSync(productionBin, 'production-firmware');
    const deviceA = 'huerta-c3-a0f262a61a68';
    const deviceB = 'huerta-c3-b0f262b61b69';
    const result = buildEvidence({
      webA: webEvidence(deviceA, 'a0f262a61a68'),
      webB: webEvidence(deviceB, 'b0f262b61b69'),
      mqttRecords: [...mqttRecords(deviceA), ...mqttRecords(deviceB)],
      developmentBin, productionBin,
    });
    assert.equal(result.automatedChecksPassed, true);
    assert.equal(result.backendDotNetRequired, false);
    assert.notEqual(result.firmware.developmentSha256, result.firmware.productionSha256);
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});

test('rechaza evidencia Web Serial que contenga secretos', () => {
  const evidence = webEvidence('huerta-c3-a0f262a61a68', 'a0f262a61a68');
  evidence.events.push({ payload: { provisioningToken: 'no-debe-guardarse' } });
  assert.throws(() => validateWebEvidence(evidence), /campos sensibles/);
});

test('rechaza dos capturas de la misma identidad física', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'huerta-phase14-'));
  try {
    const developmentBin = path.join(directory, 'development.bin');
    const productionBin = path.join(directory, 'production.bin');
    fs.writeFileSync(developmentBin, 'dev');
    fs.writeFileSync(productionBin, 'prod');
    const evidence = webEvidence('huerta-c3-a0f262a61a68', 'a0f262a61a68');
    assert.throws(() => buildEvidence({ webA: evidence, webB: evidence,
      mqttRecords: [], developmentBin, productionBin }), /identidades físicas distintas/);
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});
