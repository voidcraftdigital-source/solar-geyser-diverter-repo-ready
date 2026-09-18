import { Router } from "express";
import { db } from "../db.js";
import { hashSecret, generateDeviceId, generateDeviceSecret, generateClaimCode } from "../auth.js";

export const devRouter = Router();

// Dev/testing convenience ONLY. Lets a browser-based test client (the web
// app prototype, running self-hosted outside the Artifact sandbox) provision
// a device without needing the privileged X-Manufacturing-Key — that key
// must never live in client-side JS, so the real prototype can't call
// /api/manufacturing/devices directly. A real manufacturing line still uses
// that endpoint; this one stands in for "the label already exists" so the
// pairing UI has something real to claim.
//
// 404s unless ALLOW_DEV_PROVISIONING=true is explicitly set — disabled by
// default so this never ships live by accident.
devRouter.post("/provision-device", (req, res) => {
  if (process.env.ALLOW_DEV_PROVISIONING !== "true") {
    return res.status(404).json({ error: "not found" });
  }

  const deviceId = generateDeviceId();
  const deviceSecret = generateDeviceSecret();
  const claimCode = generateClaimCode();

  db.prepare(
    `INSERT INTO devices (device_id, device_secret_hash, claim_code_hash) VALUES (?, ?, ?)`
  ).run(deviceId, hashSecret(deviceSecret), hashSecret(claimCode));

  res.status(201).json({ device_id: deviceId, device_secret: deviceSecret, claim_code: claimCode });
});
