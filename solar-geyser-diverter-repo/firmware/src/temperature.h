#pragma once

// PT1000 temperature sensing via a MAX31865 RTD-to-digital front end —
// Specification §7. That chip was chosen specifically (see
// ../../PIN_ASSIGNMENT.md §3) for its built-in open/short fault detection,
// which this module reports through Faults::Source::SENSOR_TEMP.
namespace Temperature {
  void begin();
  void loop();

  bool hasReading();     // false until the first successful, fault-free conversion
  float celsius();       // last good reading — meaningless if hasReading() is false
  bool sensorFaulted();  // true while the MAX31865 (or a plausibility check) is faulted
}
