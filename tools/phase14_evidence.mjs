#!/usr/bin/env node

import { createHash } from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const forbiddenProductionMarkers = [
  'MQTT_CREDENTIALS_LOCAL_KEY_BEGIN', 'MQTT_CREDENTIALS_REQUEST_BEGIN',
  'PROVISIONING_REQUEST_BEGIN', 'OTA_STATUS',
];
const sensitiveNames = new Set([
  'password', 'wifipassword', 'provisioningtoken', 'bootstrapkey',
  'mqttuser', 'mqttpassword', 'authenticationtag',
]);

function parseArgs(argv) {
  const values = {};
  for (let index = 0; index < argv.length; index += 2) {
    if (!argv[index]?.startsWith('--') || argv[index + 1] === undefined)
      throw new Error(`Argumento incompleto: ${argv[index] ?? ''}`);
    values[argv[index].slice(2)] = argv[index + 1];
  }
  for (const required of ['web-a', 'web-b', 'mqtt-log', 'output'])
    if (!values[required]) throw new Error(`Falta --${required}`);
  return values;
}

function hasExposedSecret(value) {
  if (Array.isArray(value)) return value.some(hasExposedSecret);
  if (!value || typeof value !== 'object') return false;
  return Object.entries(value).some(([key, item]) =>
    (sensitiveNames.has(key.toLowerCase()) && item !== '[REDACTED]') || hasExposedSecret(item));
}

export function validateWebEvidence(evidence) {
  if (evidence?.schemaVersion !== 1 || !Array.isArray(evidence.events) || hasExposedSecret(evidence))
    throw new Error('Evidencia Web Serial inválida o con campos sensibles');
  const payloads = evidence.events.map((entry) => entry?.payload).filter(Boolean);
  const identify = payloads.find((item) => item.type === 'device_identify_result' && item.success);
  const completed = payloads.find((item) => item.type === 'device_configuration_result' &&
    item.success && item.provisioningState === 'provisioned' && item.wifiConnected === true &&
    item.mqttConnected === true && item.mqttCredentialVersion >= 1);
  if (!identify || !completed || identify.deviceId !== completed.deviceId)
    throw new Error('La evidencia no demuestra identificación y configuración completa');
  if (!identify.deviceId || !identify.hardwareId || !identify.firmwareVersion)
    throw new Error('La identidad capturada está incompleta');
  return { identify, completed };
}

export function parseMqttLog(text) {
  return text.split(/\r?\n/).filter(Boolean).map((line, index) => {
    try { return JSON.parse(line); }
    catch { throw new Error(`JSONL MQTT inválido en la línea ${index + 1}`); }
  });
}

export function validateMqttEvidence(records, deviceId) {
  const bySuffix = (suffix) => records.filter((record) =>
    record.topic === `gardens/${deviceId}/${suffix}`);
  const inventory = bySuffix('inventory').find((record) =>
    record.payload?.type === 'device_inventory' && record.payload.deviceId === deviceId &&
    Array.isArray(record.payload.devices) && record.payload.devices.length >= 6);
  const telemetry = bySuffix('telemetry').find((record) => record.payload && !record.payload.invalidJson);
  const action = bySuffix('actions-response').find((record) =>
    record.payload?.type === 'device_action_result' && record.payload.deviceId === deviceId &&
    typeof record.payload.requestId === 'string' && record.payload.requestId.length > 0);
  const ota = bySuffix('control-status').find((record) =>
    record.payload?.type === 'ota_update_status' && record.payload.deviceId === deviceId &&
    record.payload.status === 'Confirmed' && typeof record.payload.requestId === 'string' &&
    typeof record.payload.releaseId === 'string');
  return { inventory: Boolean(inventory), telemetry: Boolean(telemetry),
    action: Boolean(action), ota: Boolean(ota) };
}

function sha256(file) {
  return createHash('sha256').update(fs.readFileSync(file)).digest('hex');
}

function inspectProductionBinary(file) {
  const contents = fs.readFileSync(file).toString('latin1');
  return forbiddenProductionMarkers.filter((marker) => contents.includes(marker));
}

export function buildEvidence({ webA, webB, mqttRecords, developmentBin, productionBin }) {
  const a = validateWebEvidence(webA);
  const b = validateWebEvidence(webB);
  if (a.identify.deviceId === b.identify.deviceId ||
      a.identify.hardwareId === b.identify.hardwareId)
    throw new Error('Los dispositivos A y B no tienen identidades físicas distintas');
  if (hasExposedSecret(mqttRecords))
    throw new Error('La evidencia MQTT contiene campos sensibles sin sanitizar');
  const versionSource = fs.readFileSync(path.join(root, 'src/Constants/FirmwareVersion.h'), 'utf8');
  const versionMatch = versionSource.match(/FIRMWARE_VERSION\[\]\s*=\s*"([^"]+)"/);
  if (!versionMatch || a.identify.firmwareVersion !== versionMatch[1] ||
      b.identify.firmwareVersion !== versionMatch[1])
    throw new Error('La versión capturada no coincide con FirmwareVersion.h');
  if (a.identify.board !== 'esp32-c3-devkitm-1' || b.identify.board !== 'esp32-c3-devkitm-1')
    throw new Error('La placa capturada no corresponde al objetivo esperado');
  const productionMarkers = inspectProductionBinary(productionBin);
  const result = {
    schemaVersion: 1, generatedAt: new Date().toISOString(), backendDotNetRequired: false,
    firmware: {
      versionA: a.identify.firmwareVersion, versionB: b.identify.firmwareVersion,
      developmentSha256: sha256(developmentBin), productionSha256: sha256(productionBin),
      productionForbiddenMarkers: productionMarkers,
    },
    devices: [a.identify, b.identify],
    mqtt: {
      [a.identify.deviceId]: validateMqttEvidence(mqttRecords, a.identify.deviceId),
      [b.identify.deviceId]: validateMqttEvidence(mqttRecords, b.identify.deviceId),
    },
  };
  result.automatedChecksPassed = productionMarkers.length === 0 &&
    Object.values(result.mqtt).every((checks) => Object.values(checks).every(Boolean));
  return result;
}

function markdown(result) {
  const lines = [
    '# Evidencia de validación integral — Fase 14', '',
    `Generada: ${result.generatedAt}`, '',
    `Backend .NET requerido: ${result.backendDotNetRequired ? 'sí' : 'no'}`, '',
    '## Firmware', '',
    `- Versión A: \`${result.firmware.versionA}\``,
    `- Versión B: \`${result.firmware.versionB}\``,
    `- SHA-256 desarrollo: \`${result.firmware.developmentSha256}\``,
    `- SHA-256 producción: \`${result.firmware.productionSha256}\``,
    `- Marcadores prohibidos en producción: ${result.firmware.productionForbiddenMarkers.length ? result.firmware.productionForbiddenMarkers.join(', ') : 'ninguno'}`,
    '', '## Dispositivos', '',
    '| Dispositivo | DeviceId | HardwareId | Firmware |',
    '|---|---|---|---|',
    ...result.devices.map((device, index) =>
      `| ${index ? 'B' : 'A'} | ${device.deviceId} | ${device.hardwareId} | ${device.firmwareVersion} |`),
    '', '## Evidencia MQTT', '',
    '| DeviceId | Inventario | Telemetría | Acción correlacionada | OTA confirmada |',
    '|---|---:|---:|---:|---:|',
    ...Object.entries(result.mqtt).map(([deviceId, checks]) =>
      `| ${deviceId} | ${checks.inventory ? 'OK' : 'PENDIENTE'} | ${checks.telemetry ? 'OK' : 'PENDIENTE'} | ${checks.action ? 'OK' : 'PENDIENTE'} | ${checks.ota ? 'OK' : 'PENDIENTE'} |`),
    '', '## Resultado automatizado', '',
    result.automatedChecksPassed ? '**APROBADO**' : '**PENDIENTE O FALLIDO**', '',
    'Las pruebas destructivas, pérdidas de energía, aislamiento cruzado, revocación y',
    'operación local sin servicios se completan siguiendo la matriz física de la fase 14.',
  ];
  return `${lines.join('\n')}\n`;
}

function main(argv) {
  const options = parseArgs(argv);
  const developmentBin = path.resolve(options['development-bin'] ??
    path.join(root, '.pio/build/esp32-c3-devkitm-1/firmware.bin'));
  const productionBin = path.resolve(options['production-bin'] ??
    path.join(root, '.pio/build/esp32-c3-devkitm-1-production/firmware.bin'));
  const result = buildEvidence({
    webA: JSON.parse(fs.readFileSync(path.resolve(options['web-a']), 'utf8')),
    webB: JSON.parse(fs.readFileSync(path.resolve(options['web-b']), 'utf8')),
    mqttRecords: parseMqttLog(fs.readFileSync(path.resolve(options['mqtt-log']), 'utf8')),
    developmentBin, productionBin,
  });
  const output = path.resolve(options.output);
  fs.mkdirSync(path.dirname(output), { recursive: true });
  fs.writeFileSync(output, markdown(result));
  fs.writeFileSync(output.replace(/\.md$/i, '') + '.json', `${JSON.stringify(result, null, 2)}\n`);
  process.stdout.write(`${result.automatedChecksPassed ? 'APROBADO' : 'PENDIENTE'}: ${output}\n`);
  if (!result.automatedChecksPassed) process.exitCode = 2;
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  try { main(process.argv.slice(2)); }
  catch (error) { process.stderr.write(`${error.message}\n`); process.exitCode = 1; }
}
