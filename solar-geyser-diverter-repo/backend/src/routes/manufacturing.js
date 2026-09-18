import { Router } from "express";
import { db } from "../db.js";
import { hashSecret, generateDeviceId, generateDeviceSecret, generateClaimCode } from "../auth.js";

export const manufacturingRouter = Router();

// Gated by a static shared key (env: MANUFACTURING_KEY), not a user account —
// this represents the commissioning/manufacturing step that prints the QR +
// claim code label (PIN_ASSIGNMENT.md §3), not an end-user action. A static
// shared key is adequate for a small production run; a real manufacturing
// line at any volume should replace this with per-operator credentials —
// noted in ../README.md, not solved here.
manufacturingRouter.use((req, res, next) => {
  const key = req.get("x-manufacturing-key");
  if (!key || key !== process.env.MANUFACTURING_KEY) {
    return res.status(401).json({ error: "missing or invalid X-Manufacturing-Key" });
  }
  next();
});

// Registers a new device and returns its credentials ONCE. The device_secret
// (for the firmware's own outbound auth) and claim_code (printed on the
// label for the owner to scan/enter) are never retrievable again after this
// call — only their hashes are stored, same as a user password.
manufacturingRouter.post("/devices", (req, res) => {
  const deviceId = generateDeviceId();
  const deviceSecret = generateDeviceSecret();
  const claimCode = generateClaimCode();

  db.prepare(
    `INSERT INTO devices (device_id, device_secret_hash, claim_code_hash) VALUES (?, ?, ?)`
  ).run(deviceId, hashSecret(deviceSecret), hashSecret(claimCode));

  res.status(201).json({ device_id: deviceId, device_secret: deviceSecret, claim_code: claimCode });
});
