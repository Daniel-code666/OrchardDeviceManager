#!/usr/bin/env node

import { createCipheriv, randomBytes } from "node:crypto";
import { readFileSync } from "node:fs";
import { pathToFileURL } from "node:url";

function field(value) {
  const text = String(value);
  return `${Buffer.byteLength(text, "utf8")}:${text}\n`;
}

export function additionalAuthenticatedData(envelope) {
  return Buffer.from(
    field(envelope.contractVersion) + field(envelope.deviceId) + field(envelope.requestId),
    "utf8",
  );
}

export function buildEnvelope(request, keyHex, credentials, options = {}) {
  if (request.type !== "mqtt_credentials_request" || request.contractVersion !== 1) {
    throw new Error("La solicitud no cumple el contrato de Fase 5");
  }
  if (!/^[0-9a-f]{64}$/.test(keyHex)) {
    throw new Error("La clave local debe contener 64 caracteres hexadecimales");
  }
  if (!Number.isInteger(credentials.port) || credentials.port < 1 || credentials.port > 65535) {
    throw new Error("Puerto MQTT no valido");
  }
  if (!Number.isInteger(credentials.credentialVersion) ||
      credentials.credentialVersion <= request.currentCredentialVersion) {
    throw new Error("La version debe ser mayor que la aplicada en el dispositivo");
  }

  const envelope = {
    type: "mqtt_credentials_envelope",
    contractVersion: 1,
    deviceId: request.deviceId,
    requestId: request.requestId,
  };
  const nonce = options.nonce ?? randomBytes(12);
  const cipher = createCipheriv("aes-256-gcm", Buffer.from(keyHex, "hex"), nonce);
  cipher.setAAD(additionalAuthenticatedData(envelope));
  const plaintext = Buffer.from(JSON.stringify(credentials), "utf8");
  const ciphertext = Buffer.concat([cipher.update(plaintext), cipher.final()]);
  const tag = cipher.getAuthTag();
  if (options.invalidAuthentication) {
    tag[0] ^= 0x01;
  }
  return {
    ...envelope,
    nonce: nonce.toString("hex"),
    ciphertext: ciphertext.toString("hex"),
    authenticationTag: tag.toString("hex"),
  };
}

function parseArguments(argv) {
  const result = {};
  for (let index = 0; index < argv.length; index += 2) {
    const name = argv[index];
    if (name === "--invalid-authentication") {
      result.invalidAuthentication = true;
      index -= 1;
      continue;
    }
    if (!name?.startsWith("--") || argv[index + 1] === undefined) {
      throw new Error(`Argumento incompleto: ${name ?? ""}`);
    }
    result[name.slice(2)] = argv[index + 1];
  }
  return result;
}

function main() {
  const args = parseArguments(process.argv.slice(2));
  const request = JSON.parse(readFileSync(args["request-file"], "utf8"));
  const envelope = buildEnvelope(
    request,
    args["local-key"],
    {
      host: args.host,
      port: Number(args.port),
      clientId: args["client-id"],
      user: args.user,
      password: args.password,
      credentialVersion: Number(args.version),
    },
    { invalidAuthentication: Boolean(args.invalidAuthentication) },
  );
  process.stdout.write(`${JSON.stringify(envelope)}\n`);
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  try {
    main();
  } catch (error) {
    process.stderr.write(`${error.message}\n`);
    process.exitCode = 1;
  }
}

