#!/usr/bin/env node

import { createHmac } from 'node:crypto';
import fs from 'node:fs';
import { pathToFileURL } from 'node:url';
import path from 'node:path';

function field(value) {
  const text = String(value);
  return `${Buffer.byteLength(text, 'utf8')}:${text}\n`;
}

export function canonicalOtaCommand(command) {
  return [command.contractVersion, command.deviceId, command.requestId,
    command.commandVersion, command.releaseId, command.manifestUrl,
    command.expiresAt].map(field).join('');
}

export function signOtaCommand(command, keyHex) {
  if (!/^[a-f0-9]{64}$/.test(keyHex))
    throw new Error('La clave local debe tener 64 caracteres hexadecimales');
  return createHmac('sha256', Buffer.from(keyHex, 'hex'))
    .update(canonicalOtaCommand(command), 'utf8').digest('hex');
}

function options(argv) {
  const parsed = {};
  for (let index = 0; index < argv.length; index += 2) {
    if (!argv[index]?.startsWith('--') || argv[index + 1] === undefined)
      throw new Error(`Argumento incompleto: ${argv[index] ?? ''}`);
    parsed[argv[index].slice(2)] = argv[index + 1];
  }
  return parsed;
}

export function buildCommand(values) {
  if (!values['local-key'] && values['local-key-file']) {
    const stored = JSON.parse(fs.readFileSync(values['local-key-file'], 'utf8'));
    values['local-key'] = stored.bootstrapKey;
    if (stored.deviceId && values['device-id'] && stored.deviceId !== values['device-id'])
      throw new Error('La clave local pertenece a otro dispositivo');
  }
  const required = ['device-id', 'request-id', 'command-version', 'release-id',
    'manifest-url', 'expires-at', 'local-key'];
  for (const name of required) if (!values[name]) throw new Error(`Falta --${name}`);
  const command = {
    type: 'ota_update_request', contractVersion: 1,
    requestId: values['request-id'], deviceId: values['device-id'],
    commandVersion: Number(values['command-version']), releaseId: values['release-id'],
    manifestUrl: values['manifest-url'], expiresAt: values['expires-at'],
  };
  if (!Number.isInteger(command.commandVersion) || command.commandVersion < 1)
    throw new Error('command-version debe ser un entero positivo');
  command.authenticationTag = signOtaCommand(command, values['local-key']);
  return command;
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  try { process.stdout.write(`${JSON.stringify(buildCommand(options(process.argv.slice(2))))}\n`); }
  catch (error) { process.stderr.write(`${error.message}\n`); process.exitCode = 1; }
}

