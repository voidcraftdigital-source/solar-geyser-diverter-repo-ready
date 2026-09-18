# Geyser Diverter — Cloud Backend

The cloud backend from `../SPECIFICATION.md` §13: authenticates devices and
users, receives telemetry, and queues commands for devices to pick up. This
is what the web app prototype's pairing flow was simulating, and what the
firmware's still-unbuilt `cloud_sync` module (V0.10) will eventually talk to.

## Status: real and tested, not yet deployed

Everything here runs and is verified by an actual end-to-end test
(`npm test` — spawns the real server as a subprocess and drives it through
genuine HTTP requests: signup, device manufacture, claim, telemetry push,
command issue/delivery/ack, cross-account isolation). What it is **not** is
deployed anywhere reachable — there's no public URL, no TLS certificate, no
running instance outside this test. Getting from "runs correctly here" to
"the firmware can actually reach it from a real house" needs a deployment
step this environment can't do (see "Deploying it" below).

## Architecture

- **Node.js + Express**, using Node 22's built-in `node:sqlite` (currently
  marked experimental by Node — a harmless warning at startup, not a
  functional issue) instead of a separate database server or a native
  npm dependency. Zero external services to run for local dev; swap to
  Postgres later if device/telemetry volume ever actually needs it — nothing
  here is Postgres-hostile, `db.js` is the one file that would change.
- **Two separate credential types**, matching what `SPECIFICATION.md` §13
  and the pairing UI already committed to:
  - The **claim code** (6 digits, printed on the device label) — single-use,
    only for linking a device to an account. Cleared after use.
  - The **device secret** (32 random bytes) — the device's own ongoing
    credential for authenticating its telemetry pushes, issued once at
    "manufacturing" and never exposed again after that.
  A stolen claim code lets someone claim an *unclaimed* unit; it can't be
  used to impersonate an already-claimed device's own connection.
- **JWT** for user sessions (30-day expiry), **HTTP Basic** (device_id /
  device_secret) for device-to-backend calls — simple, standard, no session
  store needed for the user side.
- **Combined telemetry push + command pull** in one endpoint
  (`POST /api/device/telemetry`) rather than two separate round trips —
  matters on a device that's paying for its own radio time on every check-in.

## Running it

```
cp .env.example .env      # then fill in real random values for both secrets
npm install
npm start                 # listens on :8787 by default
npm test                  # full end-to-end test against a throwaway database
```

## API

**User-facing** (`Authorization: Bearer <jwt>`):

| Method | Path | Does |
|---|---|---|
| POST | `/api/auth/signup` | `{email, password}` → `{token}` |
| POST | `/api/auth/login` | `{email, password}` → `{token}` |
| GET | `/api/devices` | List this account's claimed devices, with online status + latest telemetry |
| POST | `/api/devices/claim` | `{device_id, claim_code, device_name?}` — link a device to this account |
| POST | `/api/devices/:id/unclaim` | Remove ownership (see "known gaps" — no re-claim path yet) |
| GET | `/api/devices/:id/telemetry?range=latest\|today\|7d\|30d` | Latest reading, or history over a window (Specification §19's today/7d/30d views) |
| POST | `/api/devices/:id/commands` | `{command_type, payload}` — queues a command; see the allowlist in `routes/devices.js` |

**Device-facing** (`Authorization: Basic base64(device_id:device_secret)`):

| Method | Path | Does |
|---|---|---|
| POST | `/api/device/telemetry` | Push current readings, get back any pending commands in the same response |
| POST | `/api/device/commands/:id/ack` | Mark a delivered command as applied |

**Manufacturing** (`X-Manufacturing-Key: <shared secret>`):

| Method | Path | Does |
|---|---|---|
| POST | `/api/manufacturing/devices` | Registers a device, returns `{device_id, device_secret, claim_code}` **once** — this is what gets printed on the physical label |

## Security: what's real here vs. deferred to deployment

- **TLS is deliberately not handled inside this app.** Specification §13
  requires TLS with certificate verification on the device's connection —
  that's true, but in normal practice TLS termination happens at the
  platform/reverse-proxy layer in front of a Node app (a host's built-in
  HTTPS, or nginx/Caddy with Let's Encrypt), not inside Express itself. This
  app speaks plain HTTP and must never be exposed to the internet directly
  without something terminating TLS in front of it — see "Deploying it".
- **Passwords and device secrets are hashed** (bcrypt), never stored or
  logged in plaintext. The manufacturing endpoint returns the device secret
  and claim code in plaintext exactly once, in the response body — by
  design, the same way a password is only ever plaintext at signup.
- **Cross-account isolation is real and tested**: every device-scoped query
  filters on `claimed_by_user_id = req.userId`, and the integration test
  specifically confirms a second account gets a 404, not a 403 — deliberately
  not confirming a device even exists to someone who doesn't own it.
- **Startup refuses to run with a weak or placeholder secret.**
  `JWT_SECRET`/`MANUFACTURING_KEY` under 32 characters, missing, or still the
  `.env.example` text cause the server to print why and exit(1) rather than
  quietly running insecurely — `src/validateEnv.js`, tested.
- **`qs` dependency vulnerability — fixed, not just flagged.** `npm audit`
  now reports zero vulnerabilities: `package.json` pins `qs` to `^6.16.0` via
  `overrides`, which contains the fix, without needing the breaking Express 5
  migration. Verified: full test suite still passes against the overridden
  version.
- **Login/signup rate limiting** via `express-rate-limit` (`LOGIN_RATE_LIMIT_MAX`,
  `SIGNUP_RATE_LIMIT_MAX`, `AUTH_RATE_LIMIT_WINDOW_MS` — see `.env.example`).
  Tested with a low limit to actually trigger a 429, not just assumed to work
  because the middleware is attached. If deployed behind a reverse proxy, set
  `TRUST_PROXY=true` or every client will share the proxy's rate-limit bucket.
- **CORS is now allowlist-based when configured** (`ALLOWED_ORIGINS`,
  comma-separated). Falls back to wildcard with a startup warning if unset —
  fine for local dev, meant to be set before any public deployment. Tested:
  a listed origin gets the header back, an unlisted one gets no CORS header
  at all (the browser enforces the actual block).

## Known gaps — not fixed, flagged instead of silently shipped

- **The manufacturing endpoint is gated by one static shared key**, not
  per-operator credentials or an audit trail. Adequate for a small
  production run bootstrapped by one or two people; a real manufacturing
  line at volume needs better than a single shared secret.
- **No re-claim path after unclaim/factory reset.** Unclaiming a device
  clears ownership but the claim code was already single-use-consumed at the
  original claim, so the device can't be claimed again without a fresh code
  from `/api/manufacturing/devices` (or a schema change to support
  regenerating one). Deliberate simplification, not an oversight — real
  factory-reset flow is unbuilt.
- **`ONLINE_THRESHOLD_S` (60s) is a guess**, not tuned against the firmware's
  actual future check-in interval, since `cloud_sync` doesn't exist yet.
  Revisit once it does.
- **Rate limiting is in-memory**, per server process. Fine for a single
  instance; if this ever runs as multiple replicas behind a load balancer,
  each replica tracks its own limits independently, which effectively
  multiplies the real limit by the replica count. A shared store (Redis) is
  the standard fix, not built here since there's only ever one instance so far.

## Deploying it

Needs, at minimum: a host that gives you a public HTTPS URL (Fly.io, Render,
Railway, or a VPS behind Caddy/nginx all work — none of this app is tied to
one), the two secrets in `.env` set to real random values (the server won't
start otherwise), `ALLOW_DEV_PROVISIONING` unset or `false`, `ALLOWED_ORIGINS`
set to your real frontend's origin, `TRUST_PROXY=true` if there's a reverse
proxy in front of it, and the SQLite file on persistent storage (or swap to
Postgres if the host doesn't offer a persistent disk). None of that can be
verified from here — there's no real domain or TLS certificate available in
this environment, so "it runs and the tests pass" is as far as this session
can confirm; reachability from an actual ESP32 on a real network is the next
thing to prove once it's deployed somewhere real.

### Render, specifically

`../render.yaml` (repo root) is a Blueprint that maps directly onto this
section — `rootDir` points at this folder, and the three real secrets are
marked `sync: false` so Render prompts for them once in its dashboard
rather than storing them in git.

It's set to the **free plan** on purpose — this is for testing/demoing a
prototype, not a paying customer's production backend. Two things to know
about free:

- The service sleeps after ~15 min idle, then takes ~30-50s to wake up on
  the next request. Fine for a demo, mildly annoying if it's gone cold.
- There's no persistent disk, so the SQLite database resets on every
  restart/redeploy/sleep-wake cycle — every paired device and account
  vanishes and has to be re-paired. Fine for "show it working," not fine
  for "someone relies on this being there tomorrow."

When this stops being a test and becomes a real deployment someone
actually depends on, switch `plan: free` to `plan: starter` (~$7/mo) and
uncomment the `disk:`/`DB_PATH` lines in `render.yaml` so the database
survives restarts — see the free-vs-starter trade-off comment right there
in the file.

Steps:

1. Push this repo to GitHub (already done if you're reading this from the
   repo).
2. On [render.com](https://render.com), **New +** → **Blueprint**, pick this
   repo. Render reads `render.yaml` and proposes one free service
   (`geyser-diverter-backend`).
3. When prompted, fill in the three secrets:
   - `JWT_SECRET` / `MANUFACTURING_KEY` — generate each separately with
     `node -e "console.log(require('crypto').randomBytes(48).toString('hex'))"`.
     Never reuse one for both, never reuse your local `.env` values for a
     real deployment.
   - `ALLOWED_ORIGINS` — the origin(s) that will actually call this API from
     a browser (e.g. `https://your-username.github.io`). Comma-separated,
     no trailing slash.
4. Deploy. First build takes a few minutes. Once live, confirm with:
   `curl https://<your-service>.onrender.com/api/health` → `{"status":"ok"}`.
5. Point the web app's "Backend URL" field (or the firmware's `cloud_sync`
   config, once built) at that same `https://<your-service>.onrender.com`
   URL instead of `http://localhost:8787`.

Nothing above can be verified from this session — there's no real Render
account or domain reachable from here — so treat step 4's health check as
the first real proof it worked.
