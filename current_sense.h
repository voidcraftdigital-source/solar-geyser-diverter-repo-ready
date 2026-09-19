#pragma once

// CT current + mains voltage sensing — Specification §9. Reads three CTs
// (grid/PCC, inverter/load, geyser output) and the mains voltage reference,
// each as a burst-sampled true RMS, once a second.
//
// NOT YET IMPLEMENTED: CT polarity / import-export direction. Specification
// §9 explicitly asks for it, but every reading here is an unsigned RMS
// magnitude — telling import from export needs the *sign* of real power,
// which needs synchronized voltage+current sampling, not just each
// channel's independent RMS. That's a bigger piece of work, most naturally
// built alongside V0.7's surplus-solar algorithm, which is the first stage
// that actually needs the direction rather than just the magnitude.
namespace CurrentSense {
  void begin();
  void loop();

  bool hasReading();
  float gridAmps();
  float inverterAmps();
  float geyserAmps();
  float mainsVolts();

  // V_rms * I_rms for the geyser leg only. Valid because the geyser element
  // is a resistive load (power factor ~1) — do NOT reuse this formula for
  // the grid/inverter legs, which can be reactive; true real power there
  // needs synchronized sampling and phase, not built yet.
  float geyserPowerEstimateW();

  bool overCurrent();
}
