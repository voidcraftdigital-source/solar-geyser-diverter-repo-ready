#pragma once
#include <Arduino.h>

// Half-duplex RS-485 transport over UART1 — Specification §10. Deliberately
// protocol-agnostic: this only handles the DE/RE turnaround and raw byte
// transfer. Any inverter protocol (Modbus RTU or otherwise) is built on top
// of this, not inside it — so swapping inverter brands only means a new
// protocol implementation, never touching this file.
namespace RS485Bus {
  void begin(uint32_t baud = 9600); // 9600 8N1 is the common Modbus RTU default; override per-inverter if needed

  // Blocking send — switches to transmit, waits for the UART to physically
  // finish sending (not just for the buffer to accept the bytes), then
  // switches back to receive. Half-duplex RS-485 requires this exact
  // ordering; dropping DE/RE before the last bit is actually on the wire
  // corrupts it.
  void write(const uint8_t *data, size_t len);

  int available();
  int read();
  void clearInput();

  // Sends a known byte pattern and checks it reads back within a short
  // window. Requires TX (GPIO17) jumpered directly to RX (GPIO18), or the
  // transceiver's A/B lines looped back to themselves — proves the
  // transport layer itself works without needing a real inverter attached.
  String runLoopbackTest();
}
