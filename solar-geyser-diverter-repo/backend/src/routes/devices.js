import { Router } from "express";
import { db } from "../db.js";
import { verifySecret } from "../auth.js";
import { requireUser } from "../middleware/requireUser.js";

export const devicesRouter = Router();
devicesRouter.use(requireUser);

// A device counts as online if it's checked in more recently than this —
// should be a few multiples of whatever period the firmware's cloud_sync
// module ends up polling at (Specification §13: "every 5-10s" for the
// monitoring/UX channel).
const ONLINE_THRESHOLD_S = 60;

function ownedDeviceOr404(req, res) {
  const device = db
    .prepare("SELECT * FROM devices WHERE device_id = ? AND claimed_by_user_id = ?")
    .get(req.params.deviceId, req.userId);
  if (!device) {
    res.status(404).json({ error: "no such device on this account" });
    return null;
  }
  return device;
}

function latestTelemetry(deviceId) {
  const row = db
    .prepare("SELECT * FROM telemetry WHERE device_id = ? ORDER BY recorded_at DESC LIMIT 1")
    .get(deviceId);
  if (!row) return null;
  return { recorded_at: row.recorded_at, ...JSON.parse(row.payload) };
}

function isOnline(deviceId) {
  const row = db
    .prepare("SELECT recorded_at FROM telemetry WHERE device_id = ? ORDER BY recorded_at DESC LIMIT 1")
    .get(deviceId);
  if (!row) return false;
  const ageRow = db.prepare("SELECT (julianday('now') - julianday(?)) * 86400 AS age_s").get(row.recorded_at);
  return ageRow.age_s <= ONLINE_THRESHOLD_S;
}

devicesRouter.get("/", (req, res) => {
  const devices = db
    .prepare("SELECT device_id, device_name, claimed_at FROM devices WHERE claimed_by_user_id = ?")
    .all(req.userId);
  res.json(
    devices.map((d) => ({
      device_id: d.device_id,
      device_name: d.device_name,
      claimed_at: d.claimed_at,
      online: isOnline(d.device_id),
      latest_telemetry: latestTelemetry(d.device_id),
    }))
  );
});

devicesRouter.post("/claim", (req, res) => {
  const { device_id, claim_code, device_name } = req.body || {};
  if (!device_id || !claim_code) {
    return res.status(400).json({ error: "device_id and claim_code are required" });
  }

  const device = db.prepare("SELECT * FROM devices WHERE device_id = ?").get(device_id);
  if (!device) {
    return res.status(404).json({ error: "no such device" });
  }
  if (device.claimed_by_user_id) {
    return res.status(409).json({ error: "device is already claimed" });
  }
  if (!verifySecret(claim_code, device.claim_code_hash)) {
    return res.status(401).json({ error: "incorrect claim code" });
  }

  db.prepare(
    `UPDATE devices
     SET claimed_by_user_id = ?, claimed_at = datetime('now'), claim_code_hash = NULL, device_name = ?
     WHERE device_id = ?`
  ).run(req.userId, device_name || device.device_name, device_id);

  res.json({ device_id, device_name: device_name || device.device_name });
});

devicesRouter.post("/:deviceId/unclaim", (req, res) => {
  const device = ownedDeviceOr404(req, res);
  if (!device) return;
  // Clearing claim_code_hash on claim was intentional (single-use codes) —
  // so an unclaimed device needs a fresh claim code from manufacturing/reset
  // to be claimed again. That's a deliberate simplification: real re-claim
  // (e.g. after a factory reset) is not implemented here.
  db.prepare("UPDATE devices SET claimed_by_user_id = NULL, claimed_at = NULL WHERE device_id = ?").run(
    device.device_id
  );
  res.json({ ok: true });
});

devicesRouter.get("/:deviceId/telemetry", (req, res) => {
  const device = ownedDeviceOr404(req, res);
  if (!device) return;

  const range = req.query.range || "latest";
  if (range === "latest") {
    return res.json({ latest: latestTelemetry(device.device_id), online: isOnline(device.device_id) });
  }

  const modifiers = { today: "start of day", "7d": "-7 days", "30d": "-30 days" };
  if (!(range in modifiers)) {
    return res.status(400).json({ error: "range must be one of: latest, today, 7d, 30d" });
  }
  const since =
    range === "today"
      ? db.prepare("SELECT datetime('now', 'start of day') AS s").get().s
      : db.prepare(`SELECT datetime('now', '${modifiers[range]}') AS s`).get().s;

  const rows = db
    .prepare(
      "SELECT recorded_at, payload FROM telemetry WHERE device_id = ? AND recorded_at >= ? ORDER BY recorded_at ASC"
    )
    .all(device.device_id, since);

  res.json({
    range,
    since,
    points: rows.map((r) => ({ recorded_at: r.recorded_at, ...JSON.parse(r.payload) })),
  });
});

// Commands the prototype web app's UI actually offers — kept to a known
// allowlist rather than accepting arbitrary command_type/payload from a
// user session, since this ends up driving real hardware.
const ALLOWED_COMMANDS = new Set([
  "set_mode", // payload: { mode: "auto" | "manual" | "off" | "schedule" }
  "set_target_temp", // payload: { target_c: number }
  "set_min_start_temp", // payload: { min_start_c: number }
  "set_battery_reserve", // payload: { stop_pct: number, start_pct: number }
  "set_schedule", // payload: { start: "HH:MM", end: "HH:MM" }
]);

devicesRouter.post("/:deviceId/commands", (req, res) => {
  const device = ownedDeviceOr404(req, res);
  if (!device) return;

  const { command_type, payload } = req.body || {};
  if (!ALLOWED_COMMANDS.has(command_type)) {
    return res.status(400).json({ error: `command_type must be one of: ${[...ALLOWED_COMMANDS].join(", ")}` });
  }

  const info = db
    .prepare(
      "INSERT INTO commands (device_id, issued_by_user_id, command_type, payload) VALUES (?, ?, ?, ?)"
    )
    .run(device.device_id, req.userId, command_type, JSON.stringify(payload || {}));

  res.status(201).json({ command_id: info.lastInsertRowid, status: "pending" });
});
