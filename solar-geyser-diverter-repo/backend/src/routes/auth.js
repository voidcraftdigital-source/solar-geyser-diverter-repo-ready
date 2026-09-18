import { Router } from "express";
import { db } from "../db.js";
import { hashSecret, verifySecret, signUserToken } from "../auth.js";
import { loginLimiter, signupLimiter } from "../middleware/rateLimits.js";

export const authRouter = Router();

const EMAIL_RE = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;

authRouter.post("/signup", signupLimiter, (req, res) => {
  const { email, password } = req.body || {};
  if (typeof email !== "string" || !EMAIL_RE.test(email)) {
    return res.status(400).json({ error: "a valid email is required" });
  }
  if (typeof password !== "string" || password.length < 8) {
    return res.status(400).json({ error: "password must be at least 8 characters" });
  }

  const existing = db.prepare("SELECT id FROM users WHERE email = ?").get(email);
  if (existing) {
    return res.status(409).json({ error: "an account with that email already exists" });
  }

  const info = db
    .prepare("INSERT INTO users (email, password_hash) VALUES (?, ?)")
    .run(email, hashSecret(password));

  res.status(201).json({ token: signUserToken(info.lastInsertRowid) });
});

authRouter.post("/login", loginLimiter, (req, res) => {
  const { email, password } = req.body || {};
  const user = db.prepare("SELECT * FROM users WHERE email = ?").get(email);
  if (!user || !verifySecret(password, user.password_hash)) {
    // Same error for "no such user" and "wrong password" — don't leak which one.
    return res.status(401).json({ error: "invalid email or password" });
  }
  res.json({ token: signUserToken(user.id) });
});
