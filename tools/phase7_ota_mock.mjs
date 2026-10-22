#!/usr/bin/env node

import {
  createHash,
  createPrivateKey,
  createPublicKey,
  generateKeyPairSync,
  sign,
  verify,
} from "node:crypto";
import { copyFileSync, existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { createServer } from "node:http";
import { basename, extname, join, resolve } from "node:path";

export function appendField(value) {
  const text = String(value);
  return `${Buffer.byteLength(text, "utf8")}:${text}\n`;
}

export function canonicalManifest(manifest) {
  return Buffer.from(
    [
      manifest.contractVersion,
      manifest.releaseId,
      manifest.version,
      manifest.board,
      manifest.size,
      manifest.sha256,
      manifest.firmwareUrl,
    ].map(appendField).join(""),
    "utf8",
  );
}

export function createManifest({ firmware, version, releaseId, board, firmwareUrl }, privateKeyPem) {
  const image = readFileSync(firmware);
  const manifest = {
    type: "firmware_manifest",
    contractVersion: 1,
    releaseId,
    version,
    board,
    size: image.length,
    sha256: createHash("sha256").update(image).digest("hex"),
    firmwareUrl,
  };
  manifest.signature = sign("sha256", canonicalManifest(manifest), privateKeyPem).toString("hex");
  return manifest;
}

export function verifyManifest(manifest, publicKeyPem) {
  return verify(
    "sha256",
    canonicalManifest(manifest),
    publicKeyPem,
    Buffer.from(manifest.signature, "hex"),
  );
}

function readOptions(argumentsList) {
  const options = {};
  for (let index = 0; index < argumentsList.length; index += 1) {
    const current = argumentsList[index];
    if (!current.startsWith("--")) continue;
    options[current.slice(2)] = argumentsList[index + 1];
    index += 1;
  }
  return options;
}

function initializeKeys(directory) {
  mkdirSync(directory, { recursive: true });
  const privatePath = join(directory, "ota-signing-private.pem");
  const publicPath = join(directory, "ota-signing-public.pem");
  if (existsSync(privatePath)) {
    const privateKey = createPrivateKey(readFileSync(privatePath));
    writeFileSync(publicPath, createPublicKey(privateKey).export({ type: "spki", format: "pem" }));
    return { privatePath, publicPath, created: false };
  }

  const { privateKey, publicKey } = generateKeyPairSync("ec", { namedCurve: "prime256v1" });
  writeFileSync(privatePath, privateKey.export({ type: "pkcs8", format: "pem" }), { mode: 0o600 });
  writeFileSync(publicPath, publicKey.export({ type: "spki", format: "pem" }));
  return { privatePath, publicPath, created: true };
}

function writePublicHeader(publicPath, headerPath) {
  const pem = readFileSync(publicPath, "utf8").trimEnd();
  const source = `#pragma once\n\n// Generado por tools/phase7_ota_mock.mjs. La clave privada permanece fuera del repositorio.\nconstexpr char OTA_SIGNING_PUBLIC_KEY[] = R"PEM(${pem}\n)PEM";\n`;
  writeFileSync(resolve(headerPath), source);
}

function packageRelease(options) {
  const required = ["firmware", "version", "release-id", "base-url", "output"];
  for (const name of required) {
    if (!options[name]) throw new Error(`Falta --${name}`);
  }
  const versionHeader = resolve("src/Constants/FirmwareVersion.h");
  if (existsSync(versionHeader)) {
    const match = readFileSync(versionHeader, "utf8").match(/FIRMWARE_VERSION\[\]\s*=\s*"([^"]+)"/);
    if (!match || match[1] !== options.version) {
      throw new Error(
        `La versión solicitada (${options.version}) no coincide con FirmwareVersion.h (${match?.[1] ?? "desconocida"}).`,
      );
    }
  }
  const keysDirectory = resolve(options["keys-directory"] ?? ".ota-local/keys");
  const privatePath = join(keysDirectory, "ota-signing-private.pem");
  if (!existsSync(privatePath)) throw new Error("Ejecuta primero el comando init.");

  const output = resolve(options.output);
  mkdirSync(output, { recursive: true });
  const imageName = "firmware.bin";
  const imageDestination = join(output, imageName);
  copyFileSync(resolve(options.firmware), imageDestination);
  const baseUrl = options["base-url"].replace(/\/$/, "");
  const manifest = createManifest(
    {
      firmware: imageDestination,
      version: options.version,
      releaseId: options["release-id"],
      board: options.board ?? "esp32-c3-devkitm-1",
      firmwareUrl: `${baseUrl}/${imageName}`,
    },
    readFileSync(privatePath),
  );
  writeFileSync(join(output, "manifest.json"), `${JSON.stringify(manifest, null, 2)}\n`);
  return manifest;
}

function serveDirectory(directory, port) {
  const root = resolve(directory);
  const contentTypes = { ".json": "application/json", ".bin": "application/octet-stream" };
  const server = createServer((request, response) => {
    const requestedName = basename(new URL(request.url, "http://localhost").pathname);
    if (!new Set(["manifest.json", "firmware.bin"]).has(requestedName)) {
      response.writeHead(404).end("Not found");
      return;
    }
    const target = join(root, requestedName);
    if (!existsSync(target)) {
      response.writeHead(404).end("Not found");
      return;
    }
    const content = readFileSync(target);
    response.writeHead(200, {
      "Content-Type": contentTypes[extname(target)] ?? "application/octet-stream",
      "Content-Length": content.length,
      "Cache-Control": "no-store",
    });
    response.end(content);
  });
  server.listen(port, "0.0.0.0", () => {
    process.stdout.write(`Servidor OTA local en http://0.0.0.0:${port}/manifest.json\n`);
  });
}

function main() {
  const [command, ...rest] = process.argv.slice(2);
  const options = readOptions(rest);
  if (command === "init") {
    const result = initializeKeys(resolve(options.directory ?? ".ota-local/keys"));
    if (options.header) writePublicHeader(result.publicPath, options.header);
    process.stdout.write(`${result.created ? "Claves creadas" : "Clave publica regenerada"}: ${result.publicPath}\n`);
  } else if (command === "package") {
    const manifest = packageRelease(options);
    process.stdout.write(`Release firmada: ${manifest.releaseId} (${manifest.version})\n`);
  } else if (command === "serve") {
    if (!options.directory) throw new Error("Falta --directory");
    serveDirectory(options.directory, Number(options.port ?? 8080));
  } else {
    throw new Error("Uso: phase7_ota_mock.mjs init|package|serve [opciones]");
  }
}

if (process.argv[1] && resolve(process.argv[1]) === resolve(new URL(import.meta.url).pathname.replace(/^\/(.:)/, "$1"))) {
  try {
    main();
  } catch (error) {
    process.stderr.write(`${error.message}\n`);
    process.exitCode = 1;
  }
}
