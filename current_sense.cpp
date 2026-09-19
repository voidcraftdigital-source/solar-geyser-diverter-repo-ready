#include "current_sense.h"
#include "pins.h"
#include "faults.h"
#include <Arduino.h>
#include <math.h>

namespace {
  // 640 samples comfortably spans multiple 50Hz mains cycles regardless of
  // exact per-read timing, without costing meaningful time against the
  // watchdog's 5s timeout (this whole burst, all four channels, is on the
  // order of tens of milliseconds — feed the watchdog around it, not during
  // it, since System::loop() already does that every main-loop iteration).
  constexpr int SAMPLE_COUNT = 640;
  float sampleBuf[SAMPLE_COUNT]; // static storage — kept off the stack on purpose

  // Calibration placeholders — MUST be measured against real hardware with a
  // known load before these numbers mean anything. See ../../PIN_ASSIGNMENT.md
  // §2 and Specification §30's "calibration procedure" deliverable.
  constexpr float CT_AMPS_PER_VOLT = 30.0;       // same conditioning assumed for all 3 CTs
  constexpr float MAINS_VOLTS_PER_VOLT = 300.0;  // AC_VOLTAGE_SENSE

  constexpr float OVERCURRENT_LIMIT_A = 16.0; // Specification §8 hard ceiling
  constexpr uint32_t SENSE_PERIOD_MS = 1000;

  uint32_t lastSense = 0;
  float gridA = 0, inverterA = 0, geyserA = 0, mainsV = 0;
  bool gotReading = false;
  bool overCurrentFlag = false;

  // True RMS over a burst of samples, self-centering on the burst's own mean
  // rather than assuming an exact bias voltage — more robust against real
  // component tolerances in the signal-conditioning network than trusting a
  // fixed "VDD/2" constant would be.
  float sampleRmsVolts(int pin){
    float sum = 0;
    for (int i = 0; i < SAMPLE_COUNT; i++){
      sampleBuf[i] = analogReadMilliVolts(pin) / 1000.0f;
      sum += sampleBuf[i];
    }
    float mean = sum / SAMPLE_COUNT;
    float sumSq = 0;
    for (int i = 0; i < SAMPLE_COUNT; i++){
      float d = sampleBuf[i] - mean;
      sumSq += d * d;
    }
    return sqrtf(sumSq / SAMPLE_COUNT);
  }
}

void CurrentSense::begin(){
  analogSetAttenuation(ADC_11db); // full 0-3.3V range on all ADC1 channels
  Serial.println("[CURRENT] CT/voltage sensing initialized — calibration constants are PLACEHOLDERS, see README");
}

void CurrentSense::loop(){
  uint32_t now = millis();
  if (now - lastSense < SENSE_PERIOD_MS) return;
  lastSense = now;

  gridA     = sampleRmsVolts(Pins::CT1_GRID_SENSE)     * CT_AMPS_PER_VOLT;
  inverterA = sampleRmsVolts(Pins::CT2_INVERTER_SENSE) * CT_AMPS_PER_VOLT;
  geyserA   = sampleRmsVolts(Pins::CT3_GEYSER_SENSE)   * CT_AMPS_PER_VOLT;
  mainsV    = sampleRmsVolts(Pins::AC_VOLTAGE_SENSE)   * MAINS_VOLTS_PER_VOLT;
  gotReading = true;

  if (geyserA > OVERCURRENT_LIMIT_A){
    if (!overCurrentFlag){
      overCurrentFlag = true;
      Faults::raise(Faults::Source::OVER_CURRENT, "geyser current exceeded the 16A hard limit");
    }
  } else if (overCurrentFlag){
    overCurrentFlag = false;
    Faults::clear(Faults::Source::OVER_CURRENT);
  }
}

bool CurrentSense::hasReading(){ return gotReading; }
float CurrentSense::gridAmps(){ return gridA; }
float CurrentSense::inverterAmps(){ return inverterA; }
float CurrentSense::geyserAmps(){ return geyserA; }
float CurrentSense::mainsVolts(){ return mainsV; }
float CurrentSense::geyserPowerEstimateW(){ return mainsV * geyserA; }
bool CurrentSense::overCurrent(){ return overCurrentFlag; }
