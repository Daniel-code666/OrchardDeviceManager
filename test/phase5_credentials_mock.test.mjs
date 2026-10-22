import assert from "node:assert/strict";
import { createDecipheriv } from "node:crypto";
import test from "node:test";

import {
  additionalAuthenticatedData,
  buildEnvelope,
} from "../tools/phase5_credentials_mock.mjs";

const request = {
  type: "mqtt_credentials_request",
  contractVersion: 1,
  deviceId: "huerta-esp32-test",
  requestId: "0123456789abcdef0123456789abcdef",
  currentCredentialVersion: 0,
};
const keyHex = "00".repeat(32);
const credentials = {
  host: "192.168.0.13",
  port: 1884,
  clientId: "mqtt-huerta-esp32-test",
  user: "device-huerta-esp32-test",
  password: "test-password",
  credentialVersion: 1,
};

function decrypt(envelope) {
  const decipher = createDecipheriv(
    "aes-256-gcm",
    Buffer.from(keyHex, "hex"),
    Buffer.from(envelope.nonce, "hex"),
  );
  decipher.setAAD(additionalAuthenticatedData(envelope));
  decipher.setAuthTag(Buffer.from(envelope.authenticationTag, "hex"));
  return Buffer.concat([
    decipher.update(Buffer.from(envelope.ciphertext, "hex")),
    decipher.final(),
  ]).toString("utf8");
}

test("cifra y autentica las credenciales", () => {
  const envelope = buildEnvelope(request, keyHex, credentials, {
    nonce: Buffer.alloc(12, 1),
  });
  assert.deepEqual(JSON.parse(decrypt(envelope)), credentials);
});

test("la autenticacion liga el paquete al RequestId", () => {
  const envelope = buildEnvelope(request, keyHex, credentials, {
    nonce: Buffer.alloc(12, 2),
  });
  envelope.requestId = "f".repeat(32);
  assert.throws(() => decrypt(envelope));
});

test("una etiqueta alterada no puede descifrarse", () => {
  const envelope = buildEnvelope(request, keyHex, credentials, {
    nonce: Buffer.alloc(12, 3),
    invalidAuthentication: true,
  });
  assert.throws(() => decrypt(envelope));
});

test("rechaza una version repetida", () => {
  assert.throws(() =>
    buildEnvelope(
      { ...request, currentCredentialVersion: 1 },
      keyHex,
      credentials,
    ),
  );
});

