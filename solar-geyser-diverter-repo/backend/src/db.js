import { DatabaseSync } from "node:sqlite";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const dbPath = process.env.DB_PATH || path.join(__dirname, "..", "data.sqlite");

export const db = new DatabaseSync(dbPath);

// Schema — Specification §13's data model: users own devices via a claim
// step, devices push telemetry, users issue commands the device picks up
// on its next check-in. See ../README.md for the full API this sits behind.
db.exec(`
  CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    email TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
  );

  CREATE TABLE IF NOT EXISTS devices (
    device_id TEXT PRIMARY KEY,
    device_secret_hash TEXT NOT NULL,
    claim_code_hash TEXT,
    claimed_by_user_id INTEGER REFERENCES users(id),
    device_name TEXT NOT NULL DEFAULT 'Geyser Diverter',
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    claimed_at TEXT
  );

  CREATE TABLE IF NOT EXISTS telemetry (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL REFERENCES devices(device_id),
    recorded_at TEXT NOT NULL DEFAULT (datetime('now')),
    payload TEXT NOT NULL
  );
  CREATE INDEX IF NOT EXISTS idx_telemetry_device_time ON telemetry(device_id, recorded_at);

  CREATE TABLE IF NOT EXISTS commands (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL REFERENCES devices(device_id),
    issued_by_user_id INTEGER NOT NULL REFERENCES users(id),
    command_type TEXT NOT NULL,
    payload TEXT,
    status TEXT NOT NULL DEFAULT 'pending',
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    delivered_at TEXT
  );
`);
