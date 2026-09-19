#pragma once

// Single source of truth for GPIO assignments — see ../../PIN_ASSIGNMENT.md.
// Every other pin in that document beyond what's listed here is reserved for
// a later stage and isn't touched by this firmware yet.

namespace Pins {
  constexpr int LED_HEARTBEAT = 7;  // §8: slow blink = firmware alive
  constexpr int LED_FAULT     = 8;  // §8: on = fault latched
  constexpr int LED_LINK      = 9;  // §8: Wi-Fi status, driven from V0.2
  constexpr int WATCHDOG_TEST_PIN = 0;  // BOOT button — hold at power-on to test the watchdog reset path

  // §3: PT1000 via MAX31865 (SPI), added V0.3
  constexpr int TEMP_SPI_MOSI = 11;
  constexpr int TEMP_SPI_SCLK = 12;
  constexpr int TEMP_SPI_MISO = 13;
  constexpr int TEMP_SPI_CS   = 14;
  constexpr int TEMP_DRDY     = 15;  // not used yet — this stage polls instead of using the interrupt

  // §2: CT + mains voltage sensing, added V0.4 — ADC1 only (GPIO1-10).
  // ADC2 shares hardware with the Wi-Fi radio and must never be used for a
  // reading anything depends on while Wi-Fi is active (Specification §2).
  constexpr int CT1_GRID_SENSE     = 1;  // grid/PCC
  constexpr int CT2_INVERTER_SENSE = 2;  // inverter/load side
  constexpr int CT3_GEYSER_SENSE   = 4;  // geyser output — feeds the 16A hard limit (§8)
  constexpr int AC_VOLTAGE_SENSE   = 5;  // mains voltage reference

  // §5: Output control, added V0.5. Driven through isolated driver circuitry
  // per Specification §25 — the ESP32 never switches the SSR/contactor
  // directly, whatever is wired to this pin is the driver's input only.
  constexpr int SSR_ENABLE = 21;

  // §4: Inverter RS-485, added V0.6.
  constexpr int RS485_TX    = 17;
  constexpr int RS485_RX    = 18;
  constexpr int RS485_DE_RE = 16;  // active high = transmit
}
