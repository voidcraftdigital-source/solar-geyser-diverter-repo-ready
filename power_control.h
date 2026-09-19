#pragma once
#include <Arduino.h>

// Manual SSR test control — Specification §28 V0.5: "manual SSR control at
// low-risk test conditions." This is NOT the automatic control algorithm
// (that's V0.9) and it is NOT meant to drive the real geyser element — it
// exists only so the output path itself (GPIO -> isolated driver -> SSR)
// can be bench-verified before anything upstream of it exists.
//
// Safety properties enforced here regardless of what calls into it:
//   - Defaults off on boot/reset, like every other output (Specification §25).
//   - Refuses to turn on while any fault is active, and force-turns-off if a
//     fault becomes active while already on.
//   - Auto-turns-off after a short fixed timeout, so a forgotten "on" from a
//     bench test session can't stay energized indefinitely.
namespace PowerControl {
  void begin();
  void loop();

  bool requestOn();   // false (and stays off) if refused, e.g. a fault is active
  void requestOff();
  bool isOn();
  uint32_t secondsRemaining(); // 0 if off
}
