import bcrypt from "bcryptjs";
import jwt from "jsonwebtoken";
import crypto from "node:crypto";

const JWT_SECRET = process.env.JWT_SECRET;
if (!JWT_SECRET) {
  throw new Error("JWT_SECRET is not set — copy .env.example to .env and fill it in");
}

export function hashSecret(plain) {
  return bcrypt.hashSync(plain, 10);
}

export function verifySecret(plain, hash) {
  if (!plain || !hash) return false;
  return bcrypt.compareSync(plain, hash);
}

export function signUserToken(userId) {
  return jwt.sign({ sub: userId }, JWT_SECRET, { expiresIn: "30d" });
}

export function verifyUserToken(token) {
  try {
    const decoded = jwt.verify(token, JWT_SECRET);
    return decoded.sub;
  } catch {
    return null;
  }
}

// Random device id like "GD-4F21-9B08" — matches the format already used in
// the web app prototype, and the printed-label pattern from PIN_ASSIGNMENT.md §3.
export function generateDeviceId() {
  const hex = () => crypto.randomBytes(2).toString("hex").toUpperCase();
  return `GD-${hex()}-${hex()}`;
}

// 32 random bytes, hex-encoded — the device's own ongoing cloud credential
// (Specification §13: "per-device unique credential... never a shared
// secret"). Distinct from the claim code below.
export function generateDeviceSecret() {
  return crypto.randomBytes(32).toString("hex");
}

// 6-digit numeric claim code — matches the prototype UI's "claim code"
// printed alongside the QR on the device label. One-time use: claiming a
// device clears its hash so the same code can't claim it twice.
export function generateClaimCode() {
  return String(crypto.randomInt(0, 1_000_000)).padStart(6, "0");
}
