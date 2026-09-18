// Real end-to-end test: spawns the actual server as a child process against a
// throwaway database and drives it through HTTP, the same way a real client
// would — not a mocked-out unit test. Exercises the full flow: signup,
// manufacture a device, claim it, push telemetry as the device, fetch it as
// the user, issue a command, and have the device pick it up and ack it.

import { spawn } from "node:child_process";
import { once } from "node:events";
import fs from "node:fs";
import assert from "node:assert/strict";

const PORT = 8799;
const BASE = `http://127.0.0.1:${PORT}`;
const DB_PATH = new URL("./test.sqlite", import.meta.url).pathname;

function assertStatus(res, expected, context) {
  assert.equal(res.status, expected, `${context}: expected ${expected}, got ${res.status}`);
}

async function main() {
  if (fs.existsSync(DB_PATH)) fs.unlinkSync(DB_PATH);

  const server = spawn(process.execPath, [new URL("../src/server.js", import.meta.url).pathname], {
    env: {
      ...process.env,
      PORT: String(PORT),
      DB_PATH,
      JWT_SECRET: "test-jwt-secret-that-is-definitely-long-enough-1234",
      MANUFACTURING_KEY: "test-manufacturing-key-that-is-long-enough-5678",
    },
    stdio: ["ignore", "pipe", "pipe"],
  });

  let serverOutput = "";
  server.stdout.on("data", (d) => (serverOutput += d));
  server.stderr.on("data", (d) => (serverOutput += d));

  try {
    // wait for the server to report it's listening, rather than a fixed sleep
    const deadline = Date.now() + 5000;
    while (!serverOutput.includes("listening") && Date.now() < deadline) {
      await new Promise((r) => setTimeout(r, 50));
    }
    assert.ok(serverOutput.includes("listening"), "server did not report listening in time:\n" + serverOutput);

    let res = await fetch(`${BASE}/api/health`);
    assertStatus(res, 200, "health check");
    console.log("PASS: health check");

    res = await fetch(`${BASE}/api/health`);
    assert.equal(res.headers.get("access-control-allow-origin"), "*", "CORS header present for browser clients");
    console.log("PASS: CORS header present");

    res = await fetch(`${BASE}/api/dev/provision-device`, { method: "POST" });
    assertStatus(res, 404, "dev provisioning endpoint disabled by default (ALLOW_DEV_PROVISIONING unset)");
    console.log("PASS: dev provisioning disabled by default");

    res = await fetch(`${BASE}/api/auth/signup`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ email: "installer@example.com", password: "correcthorsebattery" }),
    });
    assertStatus(res, 201, "signup");
    const { token } = await res.json();
    assert.ok(token, "signup returned a token");
    console.log("PASS: signup");

    res = await fetch(`${BASE}/api/auth/signup`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ email: "installer@example.com", password: "correcthorsebattery" }),
    });
    assertStatus(res, 409, "duplicate signup rejected");
    console.log("PASS: duplicate signup rejected");

    res = await fetch(`${BASE}/api/auth/login`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ email: "installer@example.com", password: "wrong-password" }),
    });
    assertStatus(res, 401, "login with wrong password rejected");
    console.log("PASS: wrong password rejected");

    res = await fetch(`${BASE}/api/manufacturing/devices`, {
      method: "POST",
      headers: { "x-manufacturing-key": "wrong-key" },
    });
    assertStatus(res, 401, "manufacturing endpoint rejects wrong key");
    console.log("PASS: manufacturing key enforced");

    res = await fetch(`${BASE}/api/manufacturing/devices`, {
      method: "POST",
      headers: { "x-manufacturing-key": "test-manufacturing-key-that-is-long-enough-5678" },
    });
    assertStatus(res, 201, "manufacture device");
    const { device_id, device_secret, claim_code } = await res.json();
    assert.match(device_id, /^GD-[0-9A-F]{4}-[0-9A-F]{4}$/, "device_id format");
    assert.match(claim_code, /^\d{6}$/, "claim_code is 6 digits");
    console.log("PASS: manufacture device ->", device_id);

    res = await fetch(`${BASE}/api/devices`, { headers: { authorization: `Bearer ${token}` } });
    assertStatus(res, 200, "list devices before claim");
    assert.equal((await res.json()).length, 0, "no devices claimed yet");
    console.log("PASS: empty device list before claim");

    res = await fetch(`${BASE}/api/devices/claim`, {
      method: "POST",
      headers: { "content-type": "application/json", authorization: `Bearer ${token}` },
      body: JSON.stringify({ device_id, claim_code: "000000" }),
    });
    assertStatus(res, 401, "claim with wrong code rejected");
    console.log("PASS: wrong claim code rejected");

    res = await fetch(`${BASE}/api/devices/claim`, {
      method: "POST",
      headers: { "content-type": "application/json", authorization: `Bearer ${token}` },
      body: JSON.stringify({ device_id, claim_code, device_name: "Garage Geyser" }),
    });
    assertStatus(res, 200, "claim device");
    console.log("PASS: claim device");

    res = await fetch(`${BASE}/api/devices/claim`, {
      method: "POST",
      headers: { "content-type": "application/json", authorization: `Bearer ${token}` },
      body: JSON.stringify({ device_id, claim_code }),
    });
    // Already-claimed is reported as 409, not 401 — more informative than
    // "wrong code" once the device has an owner, and it's what the server
    // actually does (checked claimed_by_user_id before the code at all).
    assertStatus(res, 409, "claim code is single-use, second attempt rejected as already-claimed");
    console.log("PASS: claim code single-use enforced");

    const basicAuth = Buffer.from(`${device_id}:${device_secret}`).toString("base64");
    res = await fetch(`${BASE}/api/device/telemetry`, {
      method: "POST",
      headers: { "content-type": "application/json", authorization: `Basic ${basicAuth}` },
      body: JSON.stringify({ temperature_c: 52.3, mode: "auto", heating: true, fault_active: false }),
    });
    assertStatus(res, 201, "device pushes telemetry");
    let body = await res.json();
    assert.equal(body.commands.length, 0, "no pending commands yet");
    console.log("PASS: device telemetry push (no commands pending)");

    const badAuth = Buffer.from(`${device_id}:wrong-secret`).toString("base64");
    res = await fetch(`${BASE}/api/device/telemetry`, {
      method: "POST",
      headers: { "content-type": "application/json", authorization: `Basic ${badAuth}` },
      body: JSON.stringify({}),
    });
    assertStatus(res, 401, "device with wrong secret rejected");
    console.log("PASS: wrong device secret rejected");

    res = await fetch(`${BASE}/api/devices/${device_id}/telemetry`, {
      headers: { authorization: `Bearer ${token}` },
    });
    assertStatus(res, 200, "user fetches latest telemetry");
    body = await res.json();
    assert.equal(body.latest.temperature_c, 52.3, "telemetry round-tripped correctly");
    assert.equal(body.online, true, "device shows online right after a fresh push");
    console.log("PASS: user reads device telemetry");

    res = await fetch(`${BASE}/api/devices/${device_id}/commands`, {
      method: "POST",
      headers: { "content-type": "application/json", authorization: `Bearer ${token}` },
      body: JSON.stringify({ command_type: "set_target_temp", payload: { target_c: 62 } }),
    });
    assertStatus(res, 201, "user issues a command");
    const { command_id } = await res.json();
    console.log("PASS: command issued ->", command_id);

    res = await fetch(`${BASE}/api/devices/${device_id}/commands`, {
      method: "POST",
      headers: { "content-type": "application/json", authorization: `Bearer ${token}` },
      body: JSON.stringify({ command_type: "not_a_real_command", payload: {} }),
    });
    assertStatus(res, 400, "command allowlist rejects unknown type");
    console.log("PASS: command allowlist enforced");

    res = await fetch(`${BASE}/api/device/telemetry`, {
      method: "POST",
      headers: { "content-type": "application/json", authorization: `Basic ${basicAuth}` },
      body: JSON.stringify({ temperature_c: 52.5 }),
    });
    body = await res.json();
    assert.equal(body.commands.length, 1, "device sees the pending command on its next check-in");
    assert.equal(body.commands[0].payload.target_c, 62, "command payload round-tripped");
    console.log("PASS: device receives pending command on next telemetry push");

    res = await fetch(`${BASE}/api/device/commands/${command_id}/ack`, {
      method: "POST",
      headers: { authorization: `Basic ${basicAuth}` },
    });
    assertStatus(res, 200, "device acks command");
    console.log("PASS: device acks command");

    res = await fetch(`${BASE}/api/device/telemetry`, {
      method: "POST",
      headers: { "content-type": "application/json", authorization: `Basic ${basicAuth}` },
      body: JSON.stringify({}),
    });
    body = await res.json();
    assert.equal(body.commands.length, 0, "acked command no longer redelivered");
    console.log("PASS: acked command not redelivered");

    // A second user must not be able to see or command the first user's device.
    res = await fetch(`${BASE}/api/auth/signup`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ email: "someone-else@example.com", password: "correcthorsebattery" }),
    });
    const otherToken = (await res.json()).token;
    res = await fetch(`${BASE}/api/devices/${device_id}/telemetry`, {
      headers: { authorization: `Bearer ${otherToken}` },
    });
    assertStatus(res, 404, "other user cannot read a device they don't own");
    console.log("PASS: cross-account device isolation enforced");

    res = await fetch(`${BASE}/api/devices/${device_id}/unclaim`, {
      method: "POST",
      headers: { authorization: `Bearer ${token}` },
    });
    assertStatus(res, 200, "owner can unclaim");
    res = await fetch(`${BASE}/api/devices`, { headers: { authorization: `Bearer ${token}` } });
    assert.equal((await res.json()).length, 0, "device list empty after unclaim");
    console.log("PASS: unclaim");

    console.log("PASS: main flow complete");
  } finally {
    server.kill();
    await once(server, "exit").catch(() => {});
    if (fs.existsSync(DB_PATH)) fs.unlinkSync(DB_PATH);
  }
}

// Separate short-lived server instance with the flag actually turned on —
// confirms the endpoint really works when opted into, not just that it's
// off by default.
async function testDevProvisioningEnabled() {
  const dbPath = new URL("./test-dev.sqlite", import.meta.url).pathname;
  if (fs.existsSync(dbPath)) fs.unlinkSync(dbPath);
  const port = PORT + 1;
  const base = `http://127.0.0.1:${port}`;

  const server = spawn(process.execPath, [new URL("../src/server.js", import.meta.url).pathname], {
    env: {
      ...process.env,
      PORT: String(port),
      DB_PATH: dbPath,
      JWT_SECRET: "test-jwt-secret-that-is-definitely-long-enough-1234",
      MANUFACTURING_KEY: "test-manufacturing-key-that-is-long-enough-5678",
      ALLOW_DEV_PROVISIONING: "true",
    },
    stdio: ["ignore", "pipe", "pipe"],
  });

  let output = "";
  server.stdout.on("data", (d) => (output += d));
  server.stderr.on("data", (d) => (output += d));

  try {
    const deadline = Date.now() + 5000;
    while (!output.includes("listening") && Date.now() < deadline) {
      await new Promise((r) => setTimeout(r, 50));
    }
    assert.ok(output.includes("listening"), "second server did not start in time:\n" + output);

    const res = await fetch(`${base}/api/dev/provision-device`, { method: "POST" });
    assertStatus(res, 201, "dev provisioning works when explicitly enabled");
    const body = await res.json();
    assert.match(body.device_id, /^GD-[0-9A-F]{4}-[0-9A-F]{4}$/, "provisioned device_id format");
    assert.match(body.claim_code, /^\d{6}$/, "provisioned claim_code format");
    console.log("PASS: dev provisioning works when ALLOW_DEV_PROVISIONING=true");
  } finally {
    server.kill();
    await once(server, "exit").catch(() => {});
    if (fs.existsSync(dbPath)) fs.unlinkSync(dbPath);
  }
}

async function testWeakSecretRejected() {
  const server = spawn(process.execPath, [new URL("../src/server.js", import.meta.url).pathname], {
    env: {
      ...process.env,
      PORT: String(PORT + 2),
      DB_PATH: new URL("./test-weak.sqlite", import.meta.url).pathname,
      JWT_SECRET: "too-short",
      MANUFACTURING_KEY: "also-too-short",
    },
    stdio: ["ignore", "pipe", "pipe"],
  });

  let output = "";
  server.stdout.on("data", (d) => (output += d));
  server.stderr.on("data", (d) => (output += d));

  const [code] = await once(server, "exit");
  assert.notEqual(code, 0, "server exits non-zero on a weak/placeholder secret");
  assert.ok(!output.includes("listening"), "server never reports listening with a weak secret");
  assert.match(output, /too short|placeholder/, "error message explains why it refused to start");
  console.log("PASS: server refuses to start with a weak/placeholder secret");

  const dbPath = new URL("./test-weak.sqlite", import.meta.url).pathname;
  if (fs.existsSync(dbPath)) fs.unlinkSync(dbPath);
}

async function testRateLimiting() {
  const dbPath = new URL("./test-ratelimit.sqlite", import.meta.url).pathname;
  if (fs.existsSync(dbPath)) fs.unlinkSync(dbPath);
  const port = PORT + 3;
  const base = `http://127.0.0.1:${port}`;

  const server = spawn(process.execPath, [new URL("../src/server.js", import.meta.url).pathname], {
    env: {
      ...process.env,
      PORT: String(port),
      DB_PATH: dbPath,
      JWT_SECRET: "test-jwt-secret-that-is-definitely-long-enough-1234",
      MANUFACTURING_KEY: "test-manufacturing-key-that-is-long-enough-5678",
      LOGIN_RATE_LIMIT_MAX: "3",
      AUTH_RATE_LIMIT_WINDOW_MS: "60000",
    },
    stdio: ["ignore", "pipe", "pipe"],
  });

  let output = "";
  server.stdout.on("data", (d) => (output += d));
  server.stderr.on("data", (d) => (output += d));

  try {
    const deadline = Date.now() + 5000;
    while (!output.includes("listening") && Date.now() < deadline) {
      await new Promise((r) => setTimeout(r, 50));
    }
    assert.ok(output.includes("listening"), "rate-limit test server did not start in time:\n" + output);

    let lastStatus;
    for (let i = 0; i < 4; i++) {
      const res = await fetch(`${base}/api/auth/login`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ email: "nobody@example.com", password: "wrong" }),
      });
      lastStatus = res.status;
      if (i < 3) assert.equal(res.status, 401, `attempt ${i + 1} within the limit gets a normal auth failure`);
    }
    assert.equal(lastStatus, 429, "4th login attempt within the window is rate-limited");
    console.log("PASS: login rate limiting kicks in after LOGIN_RATE_LIMIT_MAX attempts");
  } finally {
    server.kill();
    await once(server, "exit").catch(() => {});
    if (fs.existsSync(dbPath)) fs.unlinkSync(dbPath);
  }
}

async function testCorsAllowlist() {
  const dbPath = new URL("./test-cors.sqlite", import.meta.url).pathname;
  if (fs.existsSync(dbPath)) fs.unlinkSync(dbPath);
  const port = PORT + 4;
  const base = `http://127.0.0.1:${port}`;

  const server = spawn(process.execPath, [new URL("../src/server.js", import.meta.url).pathname], {
    env: {
      ...process.env,
      PORT: String(port),
      DB_PATH: dbPath,
      JWT_SECRET: "test-jwt-secret-that-is-definitely-long-enough-1234",
      MANUFACTURING_KEY: "test-manufacturing-key-that-is-long-enough-5678",
      ALLOWED_ORIGINS: "https://allowed.example.com, https://also-allowed.example.com",
    },
    stdio: ["ignore", "pipe", "pipe"],
  });

  let output = "";
  server.stdout.on("data", (d) => (output += d));
  server.stderr.on("data", (d) => (output += d));

  try {
    const deadline = Date.now() + 5000;
    while (!output.includes("listening") && Date.now() < deadline) {
      await new Promise((r) => setTimeout(r, 50));
    }
    assert.ok(output.includes("listening"), "CORS test server did not start in time:\n" + output);

    let res = await fetch(`${base}/api/health`, { headers: { origin: "https://allowed.example.com" } });
    assert.equal(
      res.headers.get("access-control-allow-origin"),
      "https://allowed.example.com",
      "allowed origin is echoed back"
    );
    console.log("PASS: CORS allows a listed origin");

    res = await fetch(`${base}/api/health`, { headers: { origin: "https://not-allowed.example.com" } });
    assert.equal(
      res.headers.get("access-control-allow-origin"),
      null,
      "unlisted origin gets no CORS header — the browser enforces the actual block"
    );
    console.log("PASS: CORS omits the header for an origin not on the allowlist");
  } finally {
    server.kill();
    await once(server, "exit").catch(() => {});
    if (fs.existsSync(dbPath)) fs.unlinkSync(dbPath);
  }
}

main()
  .then(testDevProvisioningEnabled)
  .then(testWeakSecretRejected)
  .then(testRateLimiting)
  .then(testCorsAllowlist)
  .then(() => console.log("\nALL TESTS PASSED"))
  .catch((err) => {
    console.error("TEST FAILED:", err);
    process.exit(1);
  });
