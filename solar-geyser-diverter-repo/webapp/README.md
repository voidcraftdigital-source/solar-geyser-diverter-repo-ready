# Geyser Diverter — Web App (backend-wired)

`geyser-console-live.html` is the same UI/behavior prototype as the published
design demo, with the pairing flow, telemetry, and remote view wired to make
**real HTTP requests** to `../backend` instead of simulating everything
client-side.

## Won't work as a claude.ai Artifact — that's expected

Artifacts run in a sandbox that blocks outbound requests to anything outside
a small CDN allowlist. This file's whole point is calling your own backend at
an arbitrary URL, so it will silently fail to pair if opened via an Artifact
link. **Open the file directly** (double-click it, or `open`/`start` it), or
serve it from any static file host / `python3 -m http.server` — either way,
it runs as a normal page with no such restriction.

## Using it

1. Start the backend (`../backend`, see its README) with
   `ALLOW_DEV_PROVISIONING=true` in its `.env`.
2. Open this file in a browser.
3. Click the pairing button, top right. Step 1 has a **Backend URL** field —
   defaults to `http://localhost:8787`; change it and click **Use this URL**
   if your backend is running somewhere else, then it requests a real test
   device from the backend.
4. "Enter code manually" (or scan the QR with a camera) → create a real
   account or sign in → the device actually gets claimed against your
   backend's database.
5. Switch to **Remote** — this is a genuine round trip: the page pushes
   simulated telemetry to the backend (playing the device's role, standing
   in for the firmware's still-unbuilt `cloud_sync` module) on one timer, and
   separately fetches it back (playing the phone's role) on another. If you
   stop the backend, Remote mode shows a "can't reach the backend" banner
   rather than silently going stale.

## What's real vs. still simulated

- **Real**: signup/login, device provisioning + claiming, telemetry push,
  telemetry fetch, cross-account isolation (all backed by the actual
  database, verified in `../backend/test/integration.test.js`).
- **Still simulated**: the "device" itself — there's no real ESP32 in this
  loop, this page's own solar/battery/temperature simulation is standing in
  for it. That's intentional; the firmware side of this connection
  (`cloud_sync`, V0.10) doesn't exist yet.
- **Not wired**: issuing commands from this UI while in Remote mode (the
  write path). The backend supports it (`POST /api/devices/:id/commands`)
  and a device picks pending commands up on its next telemetry push, but
  nothing in this page's controls calls that endpoint yet — changing a
  setting while paired still only changes the local simulation.
