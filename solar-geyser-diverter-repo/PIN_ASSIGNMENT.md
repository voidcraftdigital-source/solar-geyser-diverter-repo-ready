# REV A4 — SMART SOLAR GEYSER DIVERTER

## ESP32-S3 PIN ASSIGNMENT & SOFTWARE I/O TABLE

Companion to `SPECIFICATION.md`. This is the low-voltage "software I/O map" referenced in that document's closing note — read it before wiring anything to the 230 V power stage.

### Status of this document

This is a **proposed reference assignment**, not an approved hardware document. It gives the programmer a concrete GPIO map to start firmware development against (Section 28, V0.1–V0.4 of the specification) without waiting on the final PCB.

The exact interface chips (PT1000 front-end, CT signal-conditioning, isolation/driver parts, connector types and pinout) are **left to the programmer/hardware designer to propose**, per the pattern in `SPECIFICATION.md` Sections 13 and 30 — this table shows one workable, commonly-used approach for each so the proposal has a concrete starting point. Final GPIO numbers must be confirmed against the approved schematic (Specification Section 8) before any wiring is done.

Assumes an **ESP32-S3-WROOM-1** module. Confirm the exact part number (flash/PSRAM size, quad vs. octal SPI) before finalizing — it changes which GPIOs are reserved (see below).

---

## 1. Reserved / do-not-use pins

These are unavailable or restricted regardless of application design — do not assign safety-critical or user I/O to them.

| GPIO | Reason | Notes |
|---|---|---|
| GPIO0 | Boot-mode strapping pin | Sampled at reset to select download mode. Usually already tied to the module's BOOT button. Do not use as a driven output. |
| GPIO3 | JTAG source strapping pin | Must be left floating/default at reset. |
| GPIO45 | VDD_SPI voltage strapping pin | Must be low at reset for 3.3 V flash. Do not drive externally. |
| GPIO46 | ROM message strapping pin | Must be low at reset. |
| GPIO19, GPIO20 | Native USB D−/D+ | Reserved if native USB (console/flashing) is used instead of a USB-UART bridge. Confirm against the chosen module/board. |
| GPIO26–GPIO32 | In-package SPI flash | Always reserved on WROOM-1 modules — connects to the module's internal flash die. |
| GPIO33–GPIO37 | Octal SPI PSRAM | Reserved **only** if the chosen module variant has octal PSRAM (e.g. an "R8" part). Confirm against the exact part number. |
| GPIO43, GPIO44 | Default UART0 TX/RX | Used by the on-board USB-UART bridge for flashing and the serial debug console. Keep free for programming/bring-up. |

Everything else below is chosen from the remaining pins.

---

## 2. Analog inputs — current & voltage sensing

**Use ADC1 only (GPIO1–GPIO10).** ESP32-S3's ADC2 shares hardware with the Wi-Fi radio; `analogRead()` on ADC2 pins becomes unreliable once Wi-Fi is active, and Wi-Fi is a hard requirement (Specification Section 11) — so ADC2 must not be used for any measurement the safety/control logic depends on.

Each CT and the voltage sense line need external signal-conditioning (burden resistor + bias network, or a dedicated front-end IC like a ZMPT101B for voltage) to present a 0–3.3 V signal centered at VDD/2 to the ESP32 — this conditioning board is a hardware deliverable, not firmware.

| GPIO | ADC channel | Function | Direction | Signal | Notes |
|---|---|---|---|---|---|
| GPIO1 | ADC1_CH0 | `CT1_GRID_SENSE` | Analog in | 0–3.3 V, VDD/2 bias | Grid/PCC current (Section 9) |
| GPIO2 | ADC1_CH1 | `CT2_INVERTER_SENSE` | Analog in | 0–3.3 V, VDD/2 bias | Inverter/load-side current |
| GPIO4 | ADC1_CH3 | `CT3_GEYSER_SENSE` | Analog in | 0–3.3 V, VDD/2 bias | Geyser output current — feeds the hard over-current cutoff (Section 8) |
| GPIO5 | ADC1_CH4 | `AC_VOLTAGE_SENSE` | Analog in | 0–3.3 V, VDD/2 bias | Mains voltage reference, used for RMS power calculation (Section 9) and to derive the current-limit-to-power relationship in Section 8 |

CT polarity (Section 9) is a wiring/orientation concern on the conditioning board, not a GPIO assignment — document it in the calibration procedure instead.

---

## 3. Geyser temperature (PT1000)

Recommended: an RTD-to-digital front-end IC (e.g. MAX31865) over SPI rather than a raw resistor-divider into the ADC. It gives hardware-level open-circuit and short-circuit fault detection, which maps directly onto the "temperature sensor disconnected / shorted" faults required in Specification Section 16 — an ADC-only approach would need to infer those faults from an out-of-range reading, which is less reliable.

| GPIO | Function | Direction | Notes |
|---|---|---|---|
| GPIO11 | `TEMP_SPI_MOSI` | Output | SPI to RTD front-end |
| GPIO12 | `TEMP_SPI_SCLK` | Output | SPI clock |
| GPIO13 | `TEMP_SPI_MISO` | Input | SPI from RTD front-end |
| GPIO14 | `TEMP_SPI_CS` | Output, active low | Chip select |
| GPIO15 | `TEMP_DRDY` | Input, active low | Optional data-ready interrupt |

These are digital-only pins (SPI), so the ADC2/Wi-Fi restriction above does not apply to them.

---

## 4. Inverter communication (RS-485)

| GPIO | Function | Direction | Notes |
|---|---|---|---|
| GPIO17 | `RS485_TX` (UART1 TX) | Output | To RS-485 transceiver |
| GPIO18 | `RS485_RX` (UART1 RX) | Input | From RS-485 transceiver |
| GPIO16 | `RS485_DE_RE` | Output, active high = transmit | Drives the transceiver's combined driver-enable/receiver-enable pin |

Matches the abstract inverter interface in Specification Section 10 — the transceiver and protocol are behind this UART regardless of which inverter protocol module is loaded.

---

## 5. Digital outputs

Per Specification Section 25, the ESP32 drives these through opto-isolated transistor/MOSFET driver circuitry — it never switches the SSR or contactor coils directly.

| GPIO | Function | Direction | Active state | Notes |
|---|---|---|---|---|
| GPIO21 | `SSR_ENABLE` | Output | High = SSR on | Gated by all software AND hardware safety checks |
| GPIO38 | `GRID_CONTACTOR` | Output | High = contactor commanded closed | Interlocked with `INVERTER_CONTACTOR` (Section 24) — firmware must never drive both high |
| GPIO39 | `INVERTER_CONTACTOR` | Output | High = contactor commanded closed | See above |
| GPIO40 | `BUZZER` | Output | High = sounding | Fault/alert annunciation |

**Default state on boot, reset, watchdog reset and firmware crash must be LOW (all outputs off)** — configure these as inputs with pull-downs (or otherwise de-asserted) until the firmware explicitly initializes and enables them, satisfying Section 25's safe-default requirement.

---

## 6. Contactor feedback (state verification)

To detect the "unexpected relay/contactor state" fault in Specification Section 16, the firmware needs to read back actual contactor state, not just assume the commanded state took effect. These are isolated digital inputs from each contactor's auxiliary contact.

| GPIO | Function | Direction | Active state | Notes |
|---|---|---|---|---|
| GPIO41 | `GRID_CONTACTOR_FB` | Input | High = contactor physically closed | Compare against `GRID_CONTACTOR` command each cycle |
| GPIO42 | `INVERTER_CONTACTOR_FB` | Input | High = contactor physically closed | Compare against `INVERTER_CONTACTOR` command each cycle |

A mismatch held beyond a short debounce window is a latched fault per Section 16.

---

## 7. Hardware safety input

| GPIO | Function | Direction | Active state | Notes |
|---|---|---|---|---|
| GPIO48 | `HIGH_TEMP_TRIP` | Input | **Low = tripped** | Wired active-low with an external pull-up so that a broken/disconnected wire also reads as tripped (fails safe). Must cut `SSR_ENABLE` in hardware/interrupt logic independent of the normal software temperature loop, per Section 17. |

---

## 8. Status indicators

Basic on-board indicators for the earliest bring-up milestone (Specification Section 28, V0.1), before Wi-Fi/web UI exists.

| GPIO | Function | Direction | Notes |
|---|---|---|---|
| GPIO7 | `LED_HEARTBEAT` | Output | Slow blink = firmware alive; used with the watchdog (Section 18) |
| GPIO8 | `LED_FAULT` | Output | On = a fault is latched |
| GPIO9 | `LED_LINK` | Output | On = Wi-Fi connected; blink = cloud/remote link up (Section 13) |

---

## 9. Reserved for future expansion

| GPIO | Suggested use |
|---|---|
| GPIO47 | Spare digital I/O (e.g. `SSR_FB` if the chosen SSR provides a status/feedback signal) |
| GPIO6, GPIO10 | Spare ADC1-capable pins |
| GPIO35–37 (if not consumed by PSRAM) | Spare — confirm against module variant first |
| I²C (any two spare digital pins) | Reserved for a future expansion sensor bus, not used in Rev A4 |

---

## 10. Consolidated table

| GPIO | Function | Dir | Active state | Category |
|---|---|---|---|---|
| 0 | *(reserved — boot strap / BOOT button)* | — | — | Reserved |
| 1 | CT1_GRID_SENSE | AI | — | Current sensing |
| 2 | CT2_INVERTER_SENSE | AI | — | Current sensing |
| 3 | *(reserved — JTAG strap)* | — | — | Reserved |
| 4 | CT3_GEYSER_SENSE | AI | — | Current sensing |
| 5 | AC_VOLTAGE_SENSE | AI | — | Voltage sensing |
| 6 | *(spare, ADC1)* | — | — | Expansion |
| 7 | LED_HEARTBEAT | DO | High = on | Status |
| 8 | LED_FAULT | DO | High = on | Status |
| 9 | LED_LINK | DO | High = on | Status |
| 10 | *(spare, ADC1)* | — | — | Expansion |
| 11 | TEMP_SPI_MOSI | DO | — | Temperature |
| 12 | TEMP_SPI_SCLK | DO | — | Temperature |
| 13 | TEMP_SPI_MISO | DI | — | Temperature |
| 14 | TEMP_SPI_CS | DO | Low = selected | Temperature |
| 15 | TEMP_DRDY | DI | Low = ready | Temperature |
| 16 | RS485_DE_RE | DO | High = transmit | Inverter comms |
| 17 | RS485_TX | DO | — | Inverter comms |
| 18 | RS485_RX | DI | — | Inverter comms |
| 19 | *(reserved — USB D−, if used)* | — | — | Reserved |
| 20 | *(reserved — USB D+, if used)* | — | — | Reserved |
| 21 | SSR_ENABLE | DO | High = on | Output |
| 26–32 | *(reserved — in-package flash)* | — | — | Reserved |
| 33–37 | *(reserved — octal PSRAM, if fitted)* | — | — | Reserved |
| 38 | GRID_CONTACTOR | DO | High = closed | Output |
| 39 | INVERTER_CONTACTOR | DO | High = closed | Output |
| 40 | BUZZER | DO | High = on | Output |
| 41 | GRID_CONTACTOR_FB | DI | High = closed | Feedback |
| 42 | INVERTER_CONTACTOR_FB | DI | High = closed | Feedback |
| 43 | *(reserved — UART0 TX / programming)* | — | — | Reserved |
| 44 | *(reserved — UART0 RX / programming)* | — | — | Reserved |
| 45 | *(reserved — VDD_SPI strap)* | — | — | Reserved |
| 46 | *(reserved — ROM message strap)* | — | — | Reserved |
| 47 | *(spare)* | — | — | Expansion |
| 48 | HIGH_TEMP_TRIP | DI | **Low = tripped** | Safety input |

AI = analog in, DI = digital in, DO = digital out.

---

## 11. Notes for the hardware designer / programmer

* All outputs (Section 5) go through isolated driver circuitry per Specification Section 25 — no GPIO drives an SSR, contactor coil, or mains-referenced circuit directly.
* All sensing inputs (CTs, voltage, contactor feedback) must be galvanically isolated from the 230 V side; the exact isolation components (CT ratio, opto-isolators, isolation amplifiers) are part of the hardware proposal, not this table.
* `HIGH_TEMP_TRIP` is deliberately active-low with a pull-up so an open/disconnected wire reads as a trip rather than as "safe" — carry this same fail-safe convention into any other safety-critical input the hardware design adds.
* Connector pinout (which physical header/terminal each signal lands on) is a hardware layout decision — add a "Connector pin" column to this table once the enclosure/connector layout is fixed.
* This table should be kept in the firmware repository (e.g. as a `pins.h`/`board_config` source of truth) so a future inverter-brand or hardware revision only requires updating one file, consistent with the modular architecture in Specification Section 26.
