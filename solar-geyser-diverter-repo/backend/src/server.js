import "./validateEnv.js"; // must stay the first import — validates secrets, warns on risky config
import express from "express";
import { authRouter } from "./routes/auth.js";
import { manufacturingRouter } from "./routes/manufacturing.js";
import { devicesRouter } from "./routes/devices.js";
import { deviceApiRouter } from "./routes/deviceApi.js";
import { devRouter } from "./routes/dev.js";
import "./db.js"; // runs schema init as a side effect

const app = express();

// Only needed behind a reverse proxy (nginx/Caddy, or most hosting
// platforms) — without it, express-rate-limit below sees the proxy's IP for
// every request instead of the real client's, and either rate-limits
// everyone together or no one at all. Leave unset for direct/local use.
if (process.env.TRUST_PROXY === "true") {
  app.set("trust proxy", 1);
}

app.use(express.json());

// CORS: allowlist-based when ALLOWED_ORIGINS is set (comma-separated real
// origins), wildcard otherwise for local/dev convenience — validateEnv.js
// already warned at startup if it's falling back to wildcard. Safe to allow
// broadly even so, because auth here is a Bearer/Basic header, not a
// cookie — there's no CSRF exposure from allowing an origin, only from
// allowing one that shouldn't see the response, which the allowlist covers.
const allowedOrigins = process.env.ALLOWED_ORIGINS
  ? process.env.ALLOWED_ORIGINS.split(",").map((o) => o.trim()).filter(Boolean)
  : null;

app.use((req, res, next) => {
  const origin = req.get("origin");
  if (!allowedOrigins) {
    res.header("Access-Control-Allow-Origin", "*");
  } else if (origin && allowedOrigins.includes(origin)) {
    res.header("Access-Control-Allow-Origin", origin);
    res.header("Vary", "Origin");
  }
  res.header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Manufacturing-Key");
  res.header("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  if (req.method === "OPTIONS") return res.sendStatus(204);
  next();
});

app.get("/api/health", (req, res) => res.json({ status: "ok" }));

app.use("/api/auth", authRouter);
app.use("/api/manufacturing", manufacturingRouter);
app.use("/api/devices", devicesRouter);
app.use("/api/device", deviceApiRouter); // device-facing, singular — distinct from /api/devices (user-facing)
app.use("/api/dev", devRouter);

app.use((req, res) => res.status(404).json({ error: "not found" }));

// Express's default error handler leaks stack traces to the client — this
// swaps in a version that logs the real error server-side but only ever
// tells the caller "internal error", not what actually happened.
app.use((err, req, res, next) => {
  console.error(err);
  res.status(500).json({ error: "internal error" });
});

const port = process.env.PORT || 8787;
app.listen(port, () => {
  console.log(`[server] Geyser Diverter backend listening on :${port}`);
});
