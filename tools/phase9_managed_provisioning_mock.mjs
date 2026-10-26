#!/usr/bin/env node

import crypto from 'node:crypto';
import fs from 'node:fs';
import http from 'node:http';
import https from 'node:https';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const scriptDirectory = path.dirname(fileURLToPath(import.meta.url));

export function canonicalClaimResponse(response) {
  const fields = [
    String(response.contractVersion), response.deviceId, response.requestId,
    response.decision, response.userId, response.gardenId, response.errorCode,
  ];
  return fields.map((value) => `${Buffer.byteLength(value, 'utf8')}:${value}\n`).join('');
}

export function signClaimResponse(response, claimCode) {
  return crypto.createHmac('sha256', claimCode)
    .update(canonicalClaimResponse(response), 'utf8').digest('hex');
}

export function buildApprovedResponse(registration, userId, gardenId) {
  const response = {
    type: 'claim_response', contractVersion: 1,
    deviceId: registration.deviceId, requestId: registration.requestId,
    decision: 'approved', userId, gardenId, errorCode: '',
  };
  response.authenticationTag = signClaimResponse(response, registration.claimCode);
  return response;
}

function parseArgs(argv) {
  const result = {
    host: '0.0.0.0', port: 8090, token: 'huerta-local-token-2026',
    userId: 'usuario-web-local', gardenId: 'huerta-web-local', pendingPolls: 0,
  };
  for (let index = 0; index < argv.length; index += 1) {
    const key = argv[index];
    const value = argv[index + 1];
    if (key === '--host') result.host = value;
    else if (key === '--port') result.port = Number(value);
    else if (key === '--token') result.token = value;
    else if (key === '--user-id') result.userId = value;
    else if (key === '--garden-id') result.gardenId = value;
    else if (key === '--pending-polls') result.pendingPolls = Number(value);
    else if (key === '--cert') result.cert = value;
    else if (key === '--key') result.key = value;
    else continue;
    index += 1;
  }
  if (!Number.isInteger(result.port) || result.port < 1 || result.port > 65535)
    throw new Error('Puerto no válido');
  if (result.token.length < 16) throw new Error('El token debe tener al menos 16 caracteres');
  return result;
}

function sendJson(response, statusCode, body) {
  const payload = JSON.stringify(body);
  response.writeHead(statusCode, {
    'Content-Type': 'application/json; charset=utf-8',
    'Content-Length': Buffer.byteLength(payload), 'Cache-Control': 'no-store',
  });
  response.end(payload);
}

function validRegistration(value) {
  return value?.type === 'claim_registration' && value.contractVersion === 1 &&
    typeof value.deviceId === 'string' && typeof value.hardwareId === 'string' &&
    typeof value.mqttClientId === 'string' && typeof value.board === 'string' &&
    typeof value.firmwareVersion === 'string' && typeof value.requestId === 'string' &&
    typeof value.claimCode === 'string' && /^[a-f0-9]{32}$/.test(value.requestId) &&
    /^[A-Z2-9]{12}$/.test(value.claimCode);
}

export function createMockHandler(options) {
  const sessions = new Map();
  const pagePath = path.join(scriptDirectory, 'managed-provisioning-web.html');
  return async (request, response) => {
    const url = new URL(request.url, 'http://localhost');
    if (request.method === 'GET' && url.pathname === '/') {
      const page = fs.readFileSync(pagePath);
      response.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-store' });
      response.end(page);
      return;
    }
    if (request.headers.authorization !== `Bearer ${options.token}`) {
      sendJson(response, 401, { errorCode: 'invalid_provisioning_token' });
      return;
    }
    if (request.method === 'POST' && url.pathname === '/api/device-provisioning/claims') {
      let raw = '';
      for await (const chunk of request) {
        raw += chunk;
        if (raw.length > 8192) { sendJson(response, 413, { errorCode: 'request_too_large' }); return; }
      }
      let registration;
      try { registration = JSON.parse(raw); } catch { sendJson(response, 400, { errorCode: 'invalid_json' }); return; }
      if (!validRegistration(registration)) { sendJson(response, 400, { errorCode: 'invalid_contract' }); return; }
      sessions.set(registration.requestId, {
        response: buildApprovedResponse(registration, options.userId, options.gardenId),
        pollsRemaining: options.pendingPolls,
      });
      console.log(`Registro recibido: ${registration.deviceId} | ${registration.requestId}`);
      if (options.pendingPolls > 0) sendJson(response, 202, { status: 'pending' });
      else sendJson(response, 200, sessions.get(registration.requestId).response);
      return;
    }
    const match = url.pathname.match(/^\/api\/device-provisioning\/claims\/([a-f0-9]{32})$/);
    if (request.method === 'GET' && match) {
      const session = sessions.get(match[1]);
      if (!session) { sendJson(response, 404, { status: 'pending' }); return; }
      if (session.pollsRemaining > 0) {
        session.pollsRemaining -= 1;
        sendJson(response, 202, { status: 'pending' });
      } else sendJson(response, 200, session.response);
      return;
    }
    sendJson(response, 404, { errorCode: 'not_found' });
  };
}

export function startMockServer(options) {
  const handler = createMockHandler(options);
  const server = options.cert && options.key
    ? https.createServer({ cert: fs.readFileSync(options.cert), key: fs.readFileSync(options.key) }, handler)
    : http.createServer(handler);
  server.listen(options.port, options.host, () => {
    const scheme = options.cert && options.key ? 'https' : 'http';
    console.log(`Mock fase 9: ${scheme}://${options.host}:${options.port}`);
    console.log(`Token de prueba: ${options.token}`);
    console.log(`Interfaz Web Serial: ${scheme}://localhost:${options.port}/`);
    if (scheme === 'http') console.log('HTTP se acepta únicamente porque ENABLE_LOCAL_PROVISIONING_MOCK=1.');
  });
  return server;
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  try { startMockServer(parseArgs(process.argv.slice(2))); }
  catch (error) { console.error(error.message); process.exitCode = 1; }
}

