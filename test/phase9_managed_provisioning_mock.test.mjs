import assert from 'node:assert/strict';
import http from 'node:http';
import test from 'node:test';
import { buildApprovedResponse, canonicalClaimResponse, createMockHandler, signClaimResponse } from '../tools/phase9_managed_provisioning_mock.mjs';

const registration = {
  deviceId: 'huerta-c3-a0f262a61a68', requestId: '3b1e7355eabf5a4f0e57bc1d781a2547',
  claimCode: 'SZL5U6WY83LK',
};

function postJson(port, body, token) {
  return new Promise((resolve, reject) => {
    const payload = JSON.stringify(body);
    const request = http.request({ hostname: '127.0.0.1', port,
      path: '/api/device-provisioning/claims', method: 'POST', headers: {
        'content-type': 'application/json', 'content-length': Buffer.byteLength(payload),
        ...(token ? { authorization: `Bearer ${token}` } : {}),
      } }, (response) => {
        let raw = '';
        response.on('data', (chunk) => { raw += chunk; });
        response.on('end', () => resolve({ status: response.statusCode, body: JSON.parse(raw) }));
      });
    request.on('error', reject);
    request.end(payload);
  });
}

test('firma la misma respuesta que valida el firmware', () => {
  const response = buildApprovedResponse(registration, 'usuario-web', 'huerta-web');
  assert.equal(response.type, 'claim_response');
  assert.equal(response.authenticationTag.length, 64);
  assert.equal(response.authenticationTag, signClaimResponse(response, registration.claimCode));
  assert.match(canonicalClaimResponse(response), /^1:1\n/);
});

test('la firma cambia al alterar el propietario', () => {
  const response = buildApprovedResponse(registration, 'usuario-web', 'huerta-web');
  const signature = response.authenticationTag;
  response.userId = 'otro-usuario';
  assert.notEqual(signClaimResponse(response, registration.claimCode), signature);
});

test('el endpoint exige token y entrega un claim firmado', async (context) => {
  const options = { token: 'token-local-pruebas', userId: 'usuario-web', gardenId: 'huerta-web', pendingPolls: 0 };
  const server = http.createServer(createMockHandler(options));
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  context.after(() => server.close());
  const { port } = server.address();
  const body = { type: 'claim_registration', contractVersion: 1, ...registration,
    hardwareId: 'a0f262a61a68', mqttClientId: 'mqtt-huerta-c3-a0f262a61a68',
    board: 'esp32-c3-devkitm-1', firmwareVersion: '1.6.0', expiresInSeconds: 300 };

  const unauthorized = await postJson(port, body);
  assert.equal(unauthorized.status, 401);

  const approved = await postJson(port, body, options.token);
  assert.equal(approved.status, 200);
  const response = approved.body;
  assert.equal(response.authenticationTag, signClaimResponse(response, registration.claimCode));
});

