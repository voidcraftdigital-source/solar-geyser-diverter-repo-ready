#pragma once
#include "inverter.h"

// Stand-in InverterInterface used until a real protocol is selected and
// implemented. Deliberately does not guess at a brand or a register map —
// see ../README.md for why. isOnline() is always false and poll() always
// fails, so every caller downstream is already exercising the "no inverter
// data" path against real (if empty) hardware plumbing, per Specification
// §10's fallback-mode requirement.
class NullInverter : public InverterInterface {
public:
  bool poll() override { return false; }
  bool isOnline() const override { return false; }
  float getPVPower() const override { return 0; }
  float getHouseLoad() const override { return 0; }
  float getBatterySOC() const override { return 0; }
  float getBatteryPower() const override { return 0; }
  float getGridPower() const override { return 0; }
  const char *getInverterStatus() const override { return "no protocol configured"; }
};
