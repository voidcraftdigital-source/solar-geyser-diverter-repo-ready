#pragma once

// Per-source fault tracking. Specification §16 lists a fixed set of fault
// conditions this firmware will eventually detect; sources are added here as
// the stage that can actually detect them is built. Kept as a flat enum +
// bool array on purpose — the fault list is small and fixed, not a reason to
// build a generic event bus.
//
// Real latching-with-required-acknowledgement (§16) is explicitly a V1.0
// deliverable per the staged build order — until then, a source clears its
// own fault automatically once the underlying condition goes away.

namespace Faults {
  enum class Source {
    WATCHDOG_TEST,  // V0.1 self-test only
    SENSOR_TEMP,    // V0.3: MAX31865 fault flag or an implausible reading
    OVER_CURRENT,   // V0.4: geyser-leg CT exceeded the §8 16A hard limit
    COUNT
  };

  void begin();
  void raise(Source source, const char *reason);
  void clear(Source source);

  bool active();                  // true if ANY source is currently faulted
  bool active(Source source);
}
