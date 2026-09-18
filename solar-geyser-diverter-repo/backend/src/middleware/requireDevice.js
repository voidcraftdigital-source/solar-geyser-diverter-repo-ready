import { db } from "../db.js";
import { verifySecret } from "../auth.js";

// Gates the device-facing API using HTTP Basic auth: username = device_id,
// password = the device's own secret (issued at manufacturing, distinct
// from the human-facing claim code — Specification §13's "per-device unique
// credential... issued at manufacture").
export function requireDevice(req, res, next) {
  const header = req.get("authorization") || "";
  const [scheme, encoded] = header.split(" ");
  if (scheme !== "Basic" || !encoded) {
    return res.status(401).set("WWW-Authenticate", "Basic").json({ error: "missing device credentials" });
  }

  let deviceId, deviceSecret;
  try {
    const decoded = Buffer.from(encoded, "base64").toString("utf8");
    const sep = decoded.indexOf(":");
    deviceId = decoded.slice(0, sep);
    deviceSecret = decoded.slice(sep + 1);
  } catch {
    return res.status(401).json({ error: "malformed Authorization header" });
  }

  const device = db.prepare("SELECT * FROM devices WHERE device_id = ?").get(deviceId);
  if (!device || !verifySecret(deviceSecret, device.device_secret_hash)) {
    return res.status(401).json({ error: "invalid device credentials" });
  }

  req.device = device;
  next();
}
