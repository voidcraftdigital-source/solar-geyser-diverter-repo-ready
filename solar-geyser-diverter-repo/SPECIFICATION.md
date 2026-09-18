# REV A4 — SMART SOLAR GEYSER DIVERTER

## ESP32-S3 SOFTWARE REQUIREMENTS / PROGRAMMER BRIEF

### 1. Product purpose

We are developing a **3 kW smart geyser diverter** for South African residential solar installations.

The unit is connected to a hybrid solar inverter and controls a conventional electric geyser element.

The main purpose is to use **surplus solar PV energy during the day to heat the geyser**, while protecting the battery and ensuring the house has priority.

The prototype is limited to:

* **230 V AC / 50 Hz**
* **3 kW maximum commanded geyser power**
* **16 A absolute current protection limit**
* ESP32-S3 controller
* Wi-Fi
* Temperature sensing
* Current sensing
* Inverter communication where available
* Automatic and manual operating modes

### 2. Important Rev A4 limitation

**Grid and inverter power must NOT be paralleled.**

For Rev A4, grid and inverter are treated as **two alternative sources**.

The software must therefore never command the grid and inverter source contactors ON simultaneously.

Future versions may add simultaneous grid + solar power combining, but this is **not part of Rev A4**.

---

## 3. Main operating principle

The controller continuously determines:

1. House electrical demand
2. Available PV power
3. Battery state of charge
4. Geyser temperature
5. Geyser current/power
6. Whether the geyser is allowed to heat

The priority should be:

**House loads → battery reserve/charging requirements → surplus PV → geyser**

The geyser should only receive energy that is available according to the programmed operating limits.

---

## 4. Automatic solar mode

When the unit is in **AUTO / SOLAR mode**:

### Example

The inverter reports:

* PV = 4.0 kW
* House load = 1.2 kW
* Battery = 85%

There is approximately:

**4.0 − 1.2 = 2.8 kW**

of PV available before considering any additional inverter/battery constraints.

The controller can therefore request approximately **2.8 kW maximum** for the geyser, subject to:

* Maximum geyser power = 3 kW
* Maximum current = 16 A
* Battery reserve
* Temperature limit
* Inverter limits
* Safety limits

If only 800 W is available, the controller should request approximately 800 W rather than switching the full 3 kW load on.

---

## 5. Battery protection

The user must be able to configure a **minimum battery SOC**.

Example:

**Battery reserve = 30%**

If battery SOC is below 30%:

**Geyser heating = OFF**

If battery SOC is above the reserve and sufficient PV is available:

**Geyser heating = permitted**

There should also be hysteresis to prevent the geyser rapidly switching ON/OFF around the battery threshold.

Example:

* Start heating above 35%
* Stop heating below 30%

These values must be configurable.

---

## 6. Battery charging priority

The controller must allow a configurable amount of PV power to remain available for battery charging.

Example:

PV = 4 kW
House = 1 kW
Battery charging requirement = 1 kW

The geyser should only receive the remaining available power.

The exact algorithm should be written so the thresholds can be changed in software later.

---

## 7. Geyser temperature

Use the **PT1000 temperature sensor** to measure geyser temperature.

User settings must include:

### Target temperature

Example:

**60°C**

### Minimum start temperature

Example:

**45°C**

### High-temperature safety limit

Example:

**75°C**

If temperature reaches the target:

**Reduce/stop heating.**

If temperature reaches the safety limit:

**Immediately disable heating and generate a FAULT condition.**

The software temperature limits must not replace the geyser's existing mechanical thermostat and thermal cut-out.

---

## 8. Geyser power control

Maximum intended geyser power:

**3,000 W**

Maximum absolute current:

**16 A**

At 230 V:

3,000 / 230 ≈ **13 A**

Therefore 16 A is a **protection ceiling**, not the normal operating current for a 3 kW element.

The software must have a hard maximum power/current limit.

Example:

| Available surplus | Geyser command |
| ----------------: | -------------: |
|               0 W |            OFF |
|             500 W |         ~500 W |
|              1 kW |          ~1 kW |
|              2 kW |          ~2 kW |
|            2.5 kW |        ~2.5 kW |
|             ≥3 kW |   Maximum 3 kW |

The actual power-control implementation must match the approved hardware power stage.

---

## 9. Current monitoring

There will be current transformers for monitoring.

At minimum:

* CT1 — grid/PCC
* CT2 — inverter/load side
* CT3 — geyser output

The software must:

* Read each CT
* Calculate RMS current
* Detect abnormal current
* Calculate estimated power where voltage is known
* Display current/power to the user
* Log measurements
* Shut down the geyser if an over-current condition occurs.

CT polarity must also be accounted for because import/export direction matters.

---

## 10. Inverter communication

The controller should communicate with the inverter through **RS-485 where the inverter supports an appropriate protocol**.

The software architecture should make the inverter protocol modular.

We don't want the entire program rewritten if we change inverter brands.

Create an abstract inverter interface such as:

* `getPVPower()`
* `getHouseLoad()`
* `getBatterySOC()`
* `getBatteryPower()`
* `getGridPower()`
* `getInverterStatus()`

The first inverter protocol can then implement these functions.

If communication is unavailable, the controller should enter a safe fallback mode rather than making assumptions about available PV.

---

## 11. Wi-Fi

The ESP32-S3 must support Wi-Fi configuration.

First boot should allow the installer to configure:

* Wi-Fi SSID
* Wi-Fi password
* Device name
* Installer settings

The device should reconnect automatically after power failure.

If Wi-Fi is unavailable:

**The safety controls must continue operating locally.**

Loss of Wi-Fi must **not** cause uncontrolled geyser operation.

---

## 12. User interface

For Rev A4, a **responsive web interface hosted by the ESP32** is required for **on-site** use, plus **remote/cloud access** as described in Section 13 for use from any network.

**The prototype's user interface is a web app throughout — a browser-based, mobile-responsive page, not a native/store-installed app.** This applies both locally (served by the ESP32) and remotely (served by the cloud backend in Section 13). A native mobile app is production/future scope — see Section 13.

The user should be able to open the controller from a phone, whether on the same Wi-Fi as the unit or away from it.

Main screen should display:

* Geyser temperature
* Target temperature
* Geyser power
* Geyser current
* PV power
* House load
* Battery SOC
* Battery reserve
* Current operating mode
* Source
* Heating status
* Fault status
* Connection status (local / remote / offline)

---

## 13. Remote access (any network)

### Requirement

The user must be able to view — and, subject to the safeguards below, control — the geyser diverter **from a phone on any network** (a different Wi-Fi network, cellular data, another location entirely), not only when the phone is on the same local Wi-Fi as the unit.

The Section 12 web server hosted directly on the ESP32 only answers requests from devices on the same LAN, so it cannot satisfy this on its own. Remote access requires the device to reach out to a cloud service, since the installation's home router will not normally have inbound ports opened to it.

The choice of cloud backend/hosting platform (self-hosted server, managed IoT platform, etc.) is left to the programmer to propose, along with its ongoing hosting cost and ownership terms — see the deliverable in Section 30.

### Prototype vs. production front end

* **Prototype (Rev A4): web app only.** The remote UI is a mobile-responsive web page served by the cloud backend and opened in the phone's browser (optionally "add to home screen") — no native/store-installed app is built for the prototype.
* **Production (future): native app.** A native iOS/Android app is planned for the production release. It is **out of scope for Rev A4 firmware/prototype work**, but the cloud backend's API (telemetry + command endpoints) must be a clean, documented REST/WebSocket (or equivalent) interface consumed by the prototype web app exactly as a future native app would consume it — i.e. the backend must not be built in a way that is tied to the web app and would need redesigning when the native app is added later.

### Architecture

* The ESP32-S3 keeps its local Wi-Fi/AP + on-device web server from Sections 11–12 for on-site use. **This local path must keep working with zero dependency on the internet** — it is the same path used for commissioning and for on-site fault diagnosis when the internet is down.
* In addition, the ESP32-S3 opens an **outbound**, TLS-encrypted connection to a cloud backend (e.g. MQTT over TLS, or periodic HTTPS) so it works from behind a normal home router without port-forwarding or a static IP.
* The cloud backend:
  * Authenticates each device with a unique per-device credential (not a shared/default key).
  * Receives telemetry (the same data shown on the local UI) and stores recent history.
  * Accepts commands from an authenticated user session and forwards them to the device.
  * Ties devices to user accounts via a **pairing/commissioning step**: each unit ships with a **static QR code printed on an enclosure label at manufacture** (the unit has no display of its own — Section 12's web UI is its only screen, so the code can't be shown dynamically). The label also carries the device ID and a claim code in plain text as a human-readable fallback for when a camera isn't available or convenient. The phone's camera scans this label during first-time setup; scanning is handled client-side by the web app (Section 12), not by the device.
  * The same pairing flow must let the installer or customer **create a new cloud account on the spot**, not only sign in to an existing one — first-time setup is realistically the first time most installers touch the app, so requiring a pre-existing account before a device can be claimed would be a dead end in the field.
  * Exposes its telemetry/command API cleanly enough that the prototype web app and the future native app are just two different clients of the same API (see "Prototype vs. production front end" above).
* The phone reaches the cloud backend over **any internet connection** — home Wi-Fi, other Wi-Fi, or mobile data — via the mobile-responsive web app. It never needs to be on the device's LAN.
* Telemetry is pushed/polled at a low rate (e.g. every 5–10 s) plus immediately on state changes (mode switch, fault raised/cleared, heating start/stop) — this is a monitoring/UX channel, not a control loop.

### Safety boundary (must not be weakened by adding this feature)

* A command arriving from the cloud is a **request**, never a direct actuation. The ESP32 re-validates every remote command against the same local limits, interlocks and fault checks in Sections 5, 7, 8, 16, 17 and 24 before acting on it, exactly as it would a local web/manual command.
* All local safety behavior (Sections 16–19: fault latching, hardware safety input, watchdog, safe power-up sequencing) must keep operating identically whether or not the cloud connection is up.
* Loss of internet/cloud connectivity must **never** stop or degrade the local safety controls, and must **never** by itself change the heating state. The remote UI simply shows the device as "offline" / shows its last-known telemetry with a timestamp until the connection returns.
* No feature here may require the geyser to phone home before it is allowed to heat; AUTO/MANUAL/SCHEDULE modes must continue to run correctly with only local Wi-Fi (or no Wi-Fi at all, per Section 11) available.

### Security

* TLS with certificate verification on the ESP32's outbound connection; no plaintext telemetry or control traffic.
* Per-device unique keys/credentials issued at manufacture or commissioning — never a shared secret across units. The printed claim code is one factor in claiming a device, not the device's own cloud credential — a lost/photographed label lets someone claim an unclaimed unit, not take over an already-claimed one or impersonate the device's own outbound connection.
* Remote control requires an authenticated user session tied to the paired account; no anonymous or public control endpoint.
* Installer-only settings (Section 22) remain gated the same way remotely as they are locally.

---

## 14. User modes

Provide:

### AUTO SOLAR

Automatically use available surplus PV.

### MANUAL ON

User requests heating.

Still obey:

* Temperature limits
* Current limit
* Maximum 3 kW
* Safety trips
* Battery protection settings where applicable.

### OFF

Geyser heating disabled.

### SCHEDULE

Allow the user to specify heating periods.

Example:

**12:00–17:00**

The schedule must still obey all safety limits.

---

## 15. Future grid mode

Although simultaneous grid + inverter operation is **not Rev A4**, we want the software architecture prepared for it.

Future settings may include:

**Maximum grid contribution:**

0 W
500 W
1 kW
1.5 kW
2 kW
etc.

But this must be disabled in the Rev A4 firmware unless the hardware is specifically configured and approved for it.

---

## 16. Fault conditions

The controller must immediately disable the heating output if any critical fault occurs.

Possible faults:

* Over-current
* Over-temperature
* Temperature sensor disconnected
* Temperature sensor shorted
* Invalid inverter data
* Communication failure
* Power measurement failure
* Control-board fault
* Watchdog reset
* Unexpected relay/contactor state
* Emergency/high-temperature input active

Faults should be **latched where appropriate** and require acknowledgement/reset.

---

## 17. Hardware safety input

Provide a dedicated digital input for a hardware safety trip.

For example:

**HIGH_TEMP_TRIP**

If activated:

**SSR OFF + source contactor OFF + FAULT**

This must operate independently of normal software temperature control.

---

## 18. Watchdog

Enable the ESP32 hardware/software watchdog.

If the firmware becomes unresponsive:

**Heating output must default to OFF.**

After reboot, the system must start in a safe state.

It must not automatically resume high-power heating until the operating conditions have been checked.

---

## 19. Power failure

After mains/control power is restored:

1. ESP32 boots.
2. Outputs remain OFF.
3. Sensors initialize.
4. Inverter communication initializes.
5. Safety inputs are checked.
6. Temperature is checked.
7. Battery/PV information is checked.
8. Only then may AUTO mode restart heating.

---

## 20. Data logging

The system should record at least:

* Date/time
* PV power
* House load
* Battery SOC
* Geyser power
* Geyser current
* Geyser temperature
* Operating mode
* Faults
* Heating start/stop
* Energy delivered to geyser

Preferably calculate:

**Daily geyser energy = kWh**

and provide historical data such as:

* Today
* Yesterday
* 7 days
* 30 days

---

## 21. Energy calculation

The controller should calculate approximately:

**Geyser energy = power × time**

and accumulate this in kWh.

Example:

2 kW for 3 hours:

**6 kWh**

The app should display:

> Geyser energy today: 6.0 kWh

This is important because the commercial value of the product is partly based on showing the customer how much solar energy has been diverted to hot water.

---

## 22. Settings the customer can change

At minimum:

* Target temperature
* Minimum temperature
* Maximum temperature
* Battery reserve
* Maximum geyser power
* Maximum current
* Heating schedule
* Operating mode
* Manual override
* Wi-Fi settings
* Device name
* Remote/cloud pairing (link/unlink the device from a user account, per Section 13)

Installer-only settings should include:

* CT calibration
* Voltage calibration
* Temperature calibration
* Inverter protocol
* Source selection
* Safety thresholds
* Firmware information.

---

## 23. Default settings

Suggested initial defaults:

**Maximum geyser power:** 3,000 W
**Maximum current:** 16 A
**Target temperature:** 60°C
**High-temperature shutdown:** 75°C
**Battery reserve:** 20–30%
**Mode:** AUTO SOLAR
**Grid simultaneous mode:** DISABLED
**Manual mode:** OFF after reboot

These values must be configurable but safety limits must have a hard upper boundary.

---

## 24. Source contactor logic

Rev A4 has two possible AC sources:

**GRID**

and

**INVERTER**

They must be electrically and logically interlocked.

The firmware must enforce:

```text
GRID_CONTACTOR = OFF
before
INVERTER_CONTACTOR = ON
```

and:

```text
INVERTER_CONTACTOR = OFF
before
GRID_CONTACTOR = ON
```

There must never be a software condition where both are commanded ON.

The hardware interlock remains the primary protection.

---

## 25. Output control

The ESP32 must not directly drive the SSR or contactors.

Use appropriate transistor/MOSFET/isolated driver circuitry.

Firmware outputs should be:

* SSR_ENABLE
* GRID_CONTACTOR
* INVERTER_CONTACTOR
* BUZZER

All outputs must have safe default states during:

* Boot
* Reset
* Watchdog reset
* Firmware crash
* Loss of communication.

---

## 26. Firmware architecture

Please write the software in modular sections:

```text
/main
    system
    safety
    sensors
    inverter
    power_control
    temperature
    wifi
    web_interface
    cloud_sync
    data_logging
    settings
    faults
    ota_update
```

`cloud_sync` handles the outbound cloud connection from Section 13 (telemetry publish, remote command intake) and must stay a thin, replaceable layer that only ever calls into `power_control`/`safety` through the same validated command path the local `web_interface` uses — it must not bypass any safety check.

This is important because we intend to support different inverter manufacturers later.

---

## 27. OTA firmware updates

The ESP32 should eventually support:

**OTA — Over The Air firmware updates.**

However, firmware updates must not be allowed to leave the power output in an unsafe state.

Before updating:

**Geyser OFF**

During update:

**Geyser OFF**

After update:

**Boot safe → initialize → verify → resume operation if conditions are safe.**

---

## 28. Prototype development order

Please develop the firmware in this order:

### Firmware V0.1

ESP32 boot + LEDs + watchdog.

### V0.2

Wi-Fi + web interface.

### V0.3

PT1000 temperature measurement.

### V0.4

CT/current measurement.

### V0.5

Manual SSR control at low-risk test conditions.

### V0.6

Inverter RS-485 communication.

### V0.7

PV surplus algorithm.

### V0.8

Battery protection.

### V0.9

Automatic geyser control.

### V0.10

Cloud/remote access (Section 13) — outbound telemetry, pairing, and remote command intake re-validated through the same local safety path as the manual/web controls.

### V1.0

Fault handling + data logging + complete prototype firmware.

---

## 29. Most important control algorithm

The basic algorithm should effectively be:

```text
Read PV
Read house load
Read battery SOC
Read geyser temperature
Read geyser current
Read safety inputs

IF safety fault:
    GEYSER = OFF
    SOURCE = OFF
    FAULT = ON

ELSE IF temperature >= maximum:
    GEYSER = OFF
    FAULT/temperature protection

ELSE IF temperature >= target:
    GEYSER = OFF

ELSE IF battery SOC < minimum reserve:
    GEYSER = OFF

ELSE:
    calculate available PV surplus

    available_surplus =
        PV power
        - house load
        - required battery power
        - configured reserve

    if available_surplus > minimum_start_power:
        geyser_power =
            MIN(available_surplus,
                3000 W,
                16 A limit)

    else:
        geyser = OFF
```

The programmer should refine the algorithm with filtering, hysteresis and ramp rates so that the geyser does not constantly hunt up and down when clouds pass or household loads change.

---

## 30. What I expect from the programmer

Please provide:

1. Complete ESP32-S3 source code
2. Compiled firmware
3. Pin configuration
4. Configuration file
5. Inverter communication module
6. Web app (local + remote, per Section 13) — the prototype's only front end; no native app is in scope for this phase
7. Cloud backend / pairing service required for remote access, and its hosting/ownership terms — its API must be documented well enough to support a future native app as a second client without redesign
8. Installation instructions
9. Programming instructions
10. Fault-code list
11. Calibration procedure
12. OTA update procedure
13. Backup/recovery firmware
14. Git repository/source-code ownership transferred to us
15. Documentation of all libraries and third-party/cloud services used
16. No locked or proprietary code that prevents us from modifying the firmware later.

### Intellectual property

**All source code, firmware, configuration files and custom software developed specifically for this project must belong to the product owner.**

The programmer may use open-source libraries, but must provide a list of those libraries and their licenses.

---

## Note to the programmer

**Do not start by writing the whole program.**

> First make the ESP32-S3 read the sensors and display the values on a web page. Do not connect or control the 230 V power stage until the low-voltage software has been tested.

Follow the prototype development order in Section 28 — this lets the low-voltage "brain" of the product be tested and validated before any work begins on the 230 V power stage.

See the companion **`PIN_ASSIGNMENT.md`** (GPIO number → exact function → input/output → active state) for a proposed starting reference — final GPIO numbers must be confirmed against the approved schematic before wiring.
