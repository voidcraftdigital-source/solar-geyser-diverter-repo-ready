import rateLimit from "express-rate-limit";

// Bcrypt already makes brute-forcing a password slow, but rate limiting is
// the actual defense against it — nothing before this stopped someone
// hammering /login. Both limits are configurable via env so tests can use a
// tiny window/max instead of sending hundreds of requests to prove it works.
const windowMs = Number(process.env.AUTH_RATE_LIMIT_WINDOW_MS) || 15 * 60 * 1000;

export const loginLimiter = rateLimit({
  windowMs,
  max: Number(process.env.LOGIN_RATE_LIMIT_MAX) || 10,
  standardHeaders: true,
  legacyHeaders: false,
  message: { error: "too many login attempts — try again later" },
});

export const signupLimiter = rateLimit({
  windowMs,
  max: Number(process.env.SIGNUP_RATE_LIMIT_MAX) || 20,
  standardHeaders: true,
  legacyHeaders: false,
  message: { error: "too many signup attempts — try again later" },
});
