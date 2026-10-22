import assert from "node:assert/strict";
import { generateKeyPairSync } from "node:crypto";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";

import { canonicalManifest, createManifest, verifyManifest } from "../tools/phase7_ota_mock.mjs";

function fixture() {
  const directory = mkdtempSync(join(tmpdir(), "huerta-ota-"));
  const firmware = join(directory, "firmware.bin");
  writeFileSync(firmware, Buffer.from("firmware-de-prueba"));
  const { privateKey, publicKey } = generateKeyPairSync("ec", { namedCurve: "prime256v1" });
  const manifest = createManifest(
    {
      firmware,
      version: "1.1.0",
      releaseId: "release-test-001",
      board: "esp32-c3-devkitm-1",
      firmwareUrl: "http://127.0.0.1:8080/firmware.bin",
    },
    privateKey,
  );
  return { directory, manifest, publicKey };
}

test("firma el manifiesto y liga el hash de la imagen", () => {
  const value = fixture();
  try {
    assert.equal(verifyManifest(value.manifest, value.publicKey), true);
    assert.match(value.manifest.sha256, /^[0-9a-f]{64}$/);
    assert.equal(value.manifest.size, 18);
  } finally {
    rmSync(value.directory, { recursive: true, force: true });
  }
});

test("rechaza un manifiesto alterado", () => {
  const value = fixture();
  try {
    value.manifest.version = "9.9.9";
    assert.equal(verifyManifest(value.manifest, value.publicKey), false);
  } finally {
    rmSync(value.directory, { recursive: true, force: true });
  }
});

test("la forma canonica incluye todos los campos de seguridad", () => {
  const value = fixture();
  try {
    const payload = canonicalManifest(value.manifest).toString("utf8");
    for (const expected of [
      value.manifest.releaseId,
      value.manifest.version,
      value.manifest.board,
      value.manifest.sha256,
      value.manifest.firmwareUrl,
    ]) {
      assert.ok(payload.includes(expected));
    }
  } finally {
    rmSync(value.directory, { recursive: true, force: true });
  }
});

