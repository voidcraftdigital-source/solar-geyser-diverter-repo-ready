#pragma once
#include "inverter.h"

// Owns the active InverterInterface implementation and polls it on a timer.
// main.cpp wires this into the loop like every other sensing module
// (Specification §25's architecture) — swapping which inverter protocol is
// active only means changing what this module points `active` at.
namespace InverterLink {
  void begin();
  void loop();
  const InverterInterface &inverter();
}
