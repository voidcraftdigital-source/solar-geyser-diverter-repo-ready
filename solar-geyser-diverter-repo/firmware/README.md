# Geyser Diverter Firmware — V0.6

Staged ESP32-S3 firmware per `../SPECIFICATION.md` §28's build order. Currently
implements:

> **V0.1** — ESP32 boot + LEDs + watchdog.
> **V0.2** — Wi-Fi + web interface.
> **V0.3** — PT1000 temperature measurement.
> **V0.4** — CT/current measurement.
> **V0.5** — Manual SSR control at low-risk test conditions.
> **V0.6** — Inverter RS-485 communication (transport only — see below).

**Do not wire this to the 230V power stage or the real geyser element.** V0.5
adds the first output this firmware drives (`SSR_ENABLE`), but per the
specification's own wording for this stage it exists for *low-risk test
conditions only* — an LED, a small relay module, something through the same
isolated driver you'll use in the final build, never the mains-connected
element. There is still no automatic control logic (that's V0.9) and no
tie-in between temperature/current readings and the output — turning it on
is a manual, time-limited bench action, nothing more, and it's built to
enforce that itself rather than rely on you remembering to.

## What it does

**V0.1 (system/safety):**
- Boots, sets every output pin to a safe LOW default before anything else runs.
- Arms the hardware watchdog (5s timeout) and feeds it every loop iteration.
- Blinks `LED_HEARTBEAT` (GPIO7) at 1Hz and logs the same heartbeat over serial.
- Minimal fault flag (`faults.h/.cpp`) driving `LED_FAULT` (GPIO8) — real fault
  sources plug into this in later stages (Specification §16).

**V0.2 (Wi-Fi + local web UI), added this stage:**
- `settings.h/.cpp` — Wi-Fi credentials and device name persisted in NVS
  (`Preferences`), so they survive a reboot or power loss (Specification §11).
- `wifi_manager.h/.cpp` — a non-blocking connection state machine: tries the
  saved network on boot, falls back to its own setup Wi-Fi network (SoftAP) if
  there are no saved credentials or the connection times out (15s). Drives
  `LED_LINK` (GPIO9): fast blink = setup mode, slow blink = connecting, solid
  on = connected.
- `web_ui.h/.cpp` — a local web server (port 80) with:
  - `/` — status: device name, Wi-Fi state, IP, uptime, fault state. Sensor
    rows are explicitly marked "not yet installed" rather than faking numbers.
  - `/wifi` — form to set the Wi-Fi network, password, and device name.
  - `/wifi/reset` — forget the saved network and restart into setup mode.
  - `/api/status` — the same status as JSON, for future use.

**V0.3 (PT1000 temperature), added this stage:**
- `temperature.h/.cpp` — reads the geyser temperature via a MAX31865 RTD
  front-end over SPI (Specification §7), using the
  [Adafruit MAX31865 library](https://github.com/adafruit/Adafruit_MAX31865)
  (`lib_deps` in `platformio.ini`, fetched automatically by PlatformIO).
  Polls every 250ms. Reports a fault (`Faults::Source::SENSOR_TEMP`) on any
  MAX31865 fault flag (open/short/wiring/supply) *or* a reading outside a
  plausible physical range, instead of trusting a garbage value — matching
  Specification §16's sensor-disconnected/shorted fault requirement.
- `faults.h/.cpp` reworked to track fault sources independently (a fixed
  enum + bool array, not a generic event system) rather than one shared
  flag, since temperature is now the second thing after the watchdog
  self-test that can raise/clear a fault, and they must not step on
  each other.
- The `/` status page and `/api/status` now show a real temperature reading
  instead of a placeholder.

**V0.4 (CT current + mains voltage sensing), added this stage:**
- `current_sense.h/.cpp` — reads CT1 (grid/PCC), CT2 (inverter/load), CT3
  (geyser output), and the mains voltage reference (Specification §9), each
  as a true-RMS burst of 640 samples once a second. Raises
  `Faults::Source::OVER_CURRENT` if the geyser leg exceeds the 16A hard
  ceiling (Specification §8).
- The `/` status page and `/api/status` now show real current/voltage/power
  numbers instead of the "not yet installed" placeholder.

**Not implemented in V0.4 — CT polarity / import-export direction.**
Specification §9 explicitly asks for this, but every reading here is an
*unsigned* RMS magnitude. Telling import from export needs the sign of real
power, which needs synchronized voltage+current sampling and a phase
comparison — meaningfully more work than per-channel RMS. It's deferred to
whichever stage actually needs it first, most likely V0.7 (surplus-solar
algorithm), rather than half-built here. Don't read directionality into
these numbers; they're magnitude only.

**Power estimate is geyser-leg-only, and why:** `geyserPowerEstimateW()`
computes `V_rms * I_rms`, which is only valid because the geyser element is
a resistive load (power factor ≈ 1). Do not reuse that formula for the
grid/inverter legs — a real inverter or grid connection can be reactive, and
this simplified math doesn't account for phase angle. True real-power
measurement needs synchronized sampling, not built yet.

Pin numbers come from `../PIN_ASSIGNMENT.md` §8 via `src/pins.h` — the single
place to change if your board's wiring differs.

**Confirm before trusting a reading:**
- `temperature.cpp` assumes PT1000 with a 4300Ω reference resistor and
  3-wire RTD wiring — the standard pairing, but *your* MAX31865 breakout/PCB
  may differ (2-wire, 4-wire, a different Rref).
- `current_sense.cpp`'s `CT_AMPS_PER_VOLT` and `MAINS_VOLTS_PER_VOLT` are
  **placeholders, not calibrated values** — they depend entirely on your
  CT's turns ratio, burden resistor, and voltage-sense front end, none of
  which I have real numbers for. Every current/voltage/power reading from
  this stage is meaningless until these are measured against a known load
  and corrected — see "Calibrating the CT/voltage readings" below.

Check these constants against your actual hardware before relying on any
number this firmware reports.

**V0.5 (manual SSR test control), added this stage:**
- `power_control.h/.cpp` — drives `SSR_ENABLE` (GPIO21) through whatever
  isolated driver circuitry sits between the pin and the actual SSR
  (Specification §25 — this firmware never switches the SSR/contactor
  directly). Safety properties enforced in the module itself, not left to
  the caller:
  - Defaults off on boot/reset, same as every other output.
  - `requestOn()` refuses (returns `false`, stays off) while any fault is
    active — pulls directly from the same `Faults::active()` the temperature
    and current-sensing stages already feed.
  - If a fault appears *while* it's on, it force-turns-off within one main
    loop iteration.
  - Auto-turns-off after a fixed 10-second bench-test timeout regardless of
    anything else, so a forgotten "on" can't stay energized.
- `/power` — a dedicated page with the low-risk-load warning up front, current
  state, and "Turn ON (10s test)" / "Turn OFF now" buttons. Linked from `/`.
  `/api/status` gained `ssr_on` and `ssr_seconds_remaining`.

**V0.6 (inverter RS-485) — transport layer only, no protocol implemented yet.**
Different inverter brands (Sunsynk, Deye, Victron, Growatt, SolarEdge, ...)
use incompatible RS-485 register maps and, in some cases, entirely different
protocols. Building a concrete driver means picking one and getting its
register map right — guessing would mean shipping code that could send
genuinely wrong commands to real hardware, which is worse than not building
it. So this stage builds everything that's protocol-agnostic and defers the
protocol-specific part until it's known which inverter this targets:

- `inverter.h` — the abstract `InverterInterface` Specification §10 asks for
  by name (`getPVPower()`, `getHouseLoad()`, `getBatterySOC()`,
  `getBatteryPower()`, `getGridPower()`, `getInverterStatus()`). A real
  protocol becomes a new subclass of this — nothing downstream changes.
- `null_inverter.h` — a stand-in implementation: always offline, always
  reports "no protocol configured". This is what's active right now, so
  every caller downstream is already exercising the "no inverter data" path
  Specification §10 explicitly requires ("safe fallback mode rather than
  making assumptions about available PV") against real, if empty, plumbing.
- `rs485_bus.h/.cpp` — the actual hardware transport: UART1 remapped onto
  GPIO16/17/18, handling the half-duplex DE/RE turnaround correctly (waits
  for the UART to *physically* finish transmitting, not just for the buffer
  to accept the bytes, before switching back to receive — get this wrong and
  the last byte of every message gets corrupted). This part is real and
  protocol-agnostic regardless of which inverter ends up on the other end.
- `inverter_link.h/.cpp` — owns the active `InverterInterface*` and polls it
  every 2s, following the same module pattern as temperature/current sensing.
- `/inverter` page — shows link status and a **loopback test** you can
  actually run on the bench today, without any real inverter: jumper TX
  (GPIO17) to RX (GPIO18) and it sends a known pattern and confirms it reads
  back correctly. This proves the transport layer works independent of
  whatever protocol eventually sits on top of it.

**Once you know the target inverter**, its protocol becomes a new class
implementing `InverterInterface` (e.g. `SunsynkModbusInverter`), built on top
of `rs485_bus.h`'s `write()`/`available()`/`read()`, and `inverter_link.cpp`
points `active` at it instead of `NullInverter`. Nothing else in this
codebase needs to change.

## Build and flash

Requires [PlatformIO](https://platformio.org/) (`pip install platformio`, or the
VS Code extension).

```
pio run                  # build
pio run -t upload        # flash (board connected over USB)
pio device monitor        # serial log at 115200 baud
```

`platformio.ini` targets a generic ESP32-S3 devkit (`esp32-s3-devkitc-1`). If
PlatformIO doesn't recognize your specific board, that board id still works for
almost any ESP32-S3-WROOM-1-based devkit — swap it only if you hit a real
mismatch (wrong flash size, PSRAM variant, etc).

## First boot — connecting it to your Wi-Fi

1. Flash and power up. With no saved credentials yet, it starts its own setup
   network: watch the serial log for `AP mode: SSID 'GeyserDiverter-XXXX'`.
2. On your phone/laptop, join that Wi-Fi network.
3. Browse to **http://192.168.4.1/** → **Wi-Fi & device settings**.
4. Enter your home Wi-Fi SSID/password and a device name, submit.
5. It saves the settings and switches to your network within a couple of
   seconds. Rejoin your normal Wi-Fi and check the serial log for the new IP
   address (or check your router's client list / a network scanner) — this
   firmware doesn't do mDNS (`.local` hostnames) yet, so the IP is the only way
   to find it at this stage.
6. Power-cycle the board — it should reconnect to the saved network
   automatically, no setup network this time (Specification §11).

To make it forget the network and go back to setup mode, use the "Forget
Wi-Fi" button on the `/wifi` page (or wipe flash / erase NVS).

## What you should see

**No LEDs wired yet?** The serial log carries the same information the LEDs
would show:

```
[BOOT] Geyser Diverter firmware V0.6 (+ RS-485 transport, no inverter protocol yet)
[BOOT] Outputs initialized to safe (LOW) default state
[BOOT] Watchdog armed: 5s timeout
[WIFI] No/failed credentials -> AP mode. SSID 'GeyserDiverter-3A1F', http://192.168.4.1/
[WEBUI] Local web server started on port 80
[TEMP] MAX31865 initialized (PT1000, assumed 3-wire — confirm on hardware)
[CURRENT] CT/voltage sensing initialized — calibration constants are PLACEHOLDERS, see README
[INVERTER] RS-485 bus initialized. No protocol selected yet -- using NullInverter stand-in.
[HEARTBEAT] alive, uptime=0s, fault=no
...
```

Once connected to a real network, `[WIFI] Connected. IP: 192.168.x.x` replaces
the AP-mode line, and `LED_LINK` goes solid.

## Verifying the temperature reading

This is the one thing in V0.3 actually worth checking on the bench, not just
trusting the code compiled:

1. Browse to the device's `/` page (or `/api/status`) with the PT1000 wired up.
2. At room temperature it should read roughly 18–25°C. If it's wildly off, the
   wire-count setting (2/3/4-wire) or Rref constant almost certainly doesn't
   match your actual board — check `temperature.cpp` against your hardware.
3. Unplug the PT1000 (or short its leads, briefly, if you're confident in the
   wiring) and confirm the fault status flips to FAULT with a sensible reason
   logged (`[FAULT] sensor-temp: ...`), and back to OK once reconnected —
   this is the behavior Specification §16 actually cares about, not just a
   plausible-looking number when everything's working.

## Calibrating the CT/voltage readings

The default constants in `current_sense.cpp` are not measurements of your
hardware — they're placeholders so the code has something to compile against.
Before trusting any current/voltage/power number this firmware reports:

1. Connect a known load (a resistive heater or lamp of known wattage is
   easiest) through each CT you're calibrating, and a multimeter or a
   reference meter on the same circuit if you have one.
2. Read the raw, uncalibrated value the firmware reports (`/api/status`) for
   that channel against the known-good reference.
3. `CT_AMPS_PER_VOLT` and `MAINS_VOLTS_PER_VOLT` are linear scale factors —
   `real_amps = adc_rms_volts * CT_AMPS_PER_VOLT`. Solve for the constant
   using your known load and update it.
4. Repeat across a couple of different load levels if you can — a single
   calibration point won't catch a burden resistor or CT ratio that's
   nonlinear or off by a fixed offset rather than a scale factor.

This is a manual, one-constant-per-channel calibration for now. A proper
installer-facing calibration procedure/UI (Specification §21/§30) is a later
deliverable, not this stage's job.

## Verifying the over-current fault

There's nothing driving power yet, so this only tests the *detection* path,
not an actual shutdown — but it's still worth confirming the threshold logic
fires correctly rather than trusting the code:

1. With CT3 (geyser leg) calibrated, drive it above 16A (careful — this
   means an actual real load at that current, so only do this if you have a
   safe way to source it).
2. Confirm `/` and the serial log show a FAULT with reason `over-current`,
   and that it clears once the current drops back under the limit.

## Testing the manual SSR output

With a low-risk test load wired through the isolated driver to `SSR_ENABLE`
(GPIO21) — **not** the mains-connected geyser element:

1. Browse to `/power`, click "Turn ON (10s test)". Confirm the load actually
   energizes, and that the page/`/api/status` shows the countdown.
2. Let it run out — confirm it turns off on its own at 0s, without you
   clicking anything.
3. Turn it on again, then trigger a fault (e.g. unplug the PT1000, or hold
   the watchdog test button per below) — confirm it force-turns-off
   immediately rather than waiting out the 10s.
4. With a fault active, try "Turn ON" — confirm it's refused (the page
   should say so) and the output stays off.

If any of these three don't hold on your actual hardware, don't move on to
V0.6 until they do — this is the one stage where "the code looks right" isn't
good enough, since it's the first thing standing between firmware and
whatever's actually wired to the SSR.

## Testing the RS-485 transport

There's no real inverter protocol yet, so this only proves the transport
layer (UART framing + DE/RE turnaround) works — not communication with any
actual inverter:

1. Jumper GPIO17 (TX) directly to GPIO18 (RX) — a single wire between the two
   pins on the ESP32 is enough; you don't need the RS-485 transceiver chip
   wired up for this specific test.
2. Browse to `/inverter`, click "Run RS-485 loopback test".
3. Confirm it reports `PASS`. A `FAIL` with nothing received usually means
   the jumper isn't making contact; a `FAIL` with garbled bytes back usually
   means a wiring or baud-rate mismatch — check `RS485Bus::begin()`'s default
   (9600 8N1) against what you're actually testing against, if you've since
   wired in a real transceiver.
4. If you *do* have the RS-485 transceiver and a spare device that can send
   arbitrary serial data (a USB-RS485 adapter, a second microcontroller), you
   can additionally confirm actual line signaling (A/B differential voltage,
   correct idle-high state) with a scope or logic analyzer — the loopback
   test alone only proves the ESP32 side, not the transceiver.

## Testing the watchdog

Still the one V0.1 behavior worth proving on the bench, not just trusting:

1. Hold the board's **BOOT** button (GPIO0) while it powers up or resets.
2. It logs that the test is armed, runs normally for 5 seconds, then
   deliberately hangs forever — the same failure mode as firmware that's stopped
   responding.
3. Within ~5 seconds the watchdog should reset the board on its own.

## Known simplifications in this stage (not bugs, just not built yet)

- **No mDNS/`.local` hostname.** You need the IP address from the serial log
  or your router until a later stage adds this.
- **No dual AP+STA fallback.** While attempting to (re)connect to a saved
  network, the setup Wi-Fi network is off — if the saved network is wrong or
  unreachable, it falls back to setup mode after the 15s timeout rather than
  staying reachable throughout the attempt. You'll briefly lose the connection
  either way; this just means "briefly" instead of "not at all."
- **Reconnect-after-save is deferred by ~1s** before switching networks, so the
  "saved" confirmation page has time to actually reach your browser before the
  radio switches modes — a known ESP32 web-config gotcha, not an oversight.

## Not built yet

**A real inverter protocol** — see the V0.6 section above. This is the one
gap in the staged plan that isn't just "later," it's "waiting on knowing
which inverter this targets." Everything else it needs (the interface, the
transport, the polling loop) is already there.

Beyond that, everything else in the staged plan (Specification §28): the
surplus-solar algorithm (V0.7) — which is also where CT polarity/import-export
direction lands, see above — battery protection (V0.8), automatic control
(V0.9), cloud pairing (V0.10), then full fault handling + data logging
(V1.0), including the latched, acknowledgement-required fault behavior
Specification §16 actually asks for; every fault source built so far
auto-clears once its condition goes away, which is a deliberate
simplification until V1.0, not the final behavior. V0.5's SSR control is
manual and time-limited only — there is still no connection at all between
temperature/current/inverter readings and the output; that tie-in doesn't
arrive until V0.9. Each stage gets the same treatment this one
did: build it, bench-test it on real hardware, confirm it fails safe, *then*
move to the next stage — never all at once, and never near mains power until
the stage that actually needs it says so.
