#pragma once

// Abstract inverter interface — Specification §10, matching the method
// names it lists directly. Kept as an abstract base rather than baked into
// one protocol so a different inverter brand means a new subclass, not a
// rewrite of everything downstream that reads PV/battery/grid data.
class InverterInterface {
public:
  virtual ~InverterInterface() = default;

  // Fetch fresh data over the bus. Returns false on comm failure/timeout —
  // callers must treat that as "no data available", never as zeros, per
  // Specification §10: "a safe fallback mode rather than making assumptions
  // about available PV."
  virtual bool poll() = 0;
  virtual bool isOnline() const = 0;

  virtual float getPVPower() const = 0;       // watts
  virtual float getHouseLoad() const = 0;     // watts
  virtual float getBatterySOC() const = 0;    // percent
  virtual float getBatteryPower() const = 0;  // watts: + charging / - discharging
  virtual float getGridPower() const = 0;     // watts: + import / - export
  virtual const char *getInverterStatus() const = 0;
};
