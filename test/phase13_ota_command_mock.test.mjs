import assert from 'node:assert/strict';
import test from 'node:test';
import { buildCommand, signOtaCommand } from '../tools/phase13_ota_command_mock.mjs';

const values = {
  'device-id': 'huerta-c3-a0f262a61a68', 'request-id': 'ota-001',
  'command-version': '1', 'release-id': 'release-1.8.0',
  'manifest-url': 'https://firmware.example.test/release-1.8.0/manifest.json',
  'expires-at': '2026-10-26T23:00:00Z', 'local-key': '12'.repeat(32),
};

test('firma todos los campos sensibles de la orden OTA', () => {
  const command = buildCommand(values);
  assert.equal(command.authenticationTag, signOtaCommand(command, values['local-key']));
  const changed = { ...command, releaseId: 'release-1.8.1' };
  assert.notEqual(signOtaCommand(changed, values['local-key']), command.authenticationTag);
});

test('rechaza una versión de comando no positiva', () => {
  assert.throws(() => buildCommand({ ...values, 'command-version': '0' }), /entero positivo/);
});

