#include "temperature.h"
#include "pins.h"
#include "faults.h"
#include <Adafruit_MAX31865.h>
#include <SPI.h>

namespace {
  // PT1000: 1000 ohm nominal at 0C. RREF is the precision reference resistor
  // on the MAX31865 board — 4300 ohm is the standard pairing for PT1000
  // (PT100 boards use 430 ohm instead). Confirm against the actual board.
  constexpr float RNOMINAL = 1000.0;
  constexpr float RREF = 4300.0;

  // 3-wire is assumed as the common choice for this kind of sensor run.
  // Confirm against the actual RTD wiring before trusting readings — 2-wire
  // and 4-wire need a different constant passed to rtd.begin().
  constexpr max31865_numwires_t WIRING = MAX31865_3WIRE;

  constexpr uint32_t POLL_PERIOD_MS = 250;
  // Reject anything outside a sane physical range for a geyser tank instead
  // of reporting an ADC glitch as a real reading.
  constexpr float PLAUSIBLE_MIN_C = -20.0;
  constexpr float PLAUSIBLE_MAX_C = 150.0;

  Adafruit_MAX31865 rtd(Pins::TEMP_SPI_CS);
  uint32_t lastPoll = 0;
  bool gotReading = false;
  float lastCelsius = 0.0;
  bool faulted = false;

  const char *faultDescription(uint8_t fault){
    if (fault & MAX31865_FAULT_HIGHTHRESH) return "RTD high threshold (open circuit likely)";
    if (fault & MAX31865_FAULT_LOWTHRESH)  return "RTD low threshold (short circuit likely)";
    if (fault & MAX31865_FAULT_REFINLOW)   return "REFIN- low (wiring fault)";
    if (fault & MAX31865_FAULT_REFINHIGH)  return "REFIN- high (wiring fault)";
    if (fault & MAX31865_FAULT_RTDINLOW)   return "RTDIN- low (sensor likely disconnected)";
    if (fault & MAX31865_FAULT_OVUV)       return "over/under-voltage on the MAX31865 supply";
    return "unspecified MAX31865 fault";
  }
}

void Temperature::begin(){
  SPI.begin(Pins::TEMP_SPI_SCLK, Pins::TEMP_SPI_MISO, Pins::TEMP_SPI_MOSI, -1);
  rtd.begin(WIRING);
  Serial.println("[TEMP] MAX31865 initialized (PT1000, assumed 3-wire — confirm on hardware)");
}

void Temperature::loop(){
  uint32_t now = millis();
  if (now - lastPoll < POLL_PERIOD_MS) return;
  lastPoll = now;

  uint8_t fault = rtd.readFault();
  if (fault){
    if (!faulted){
      faulted = true;
      Faults::raise(Faults::Source::SENSOR_TEMP, faultDescription(fault));
    }
    rtd.clearFault();
    return; // don't trust a temperature taken during a reported fault
  }

  float t = rtd.temperature(RNOMINAL, RREF);

  if (t < PLAUSIBLE_MIN_C || t > PLAUSIBLE_MAX_C){
    if (!faulted){
      faulted = true;
      Faults::raise(Faults::Source::SENSOR_TEMP, "reading outside plausible range");
    }
    return;
  }

  if (faulted){
    faulted = false;
    Faults::clear(Faults::Source::SENSOR_TEMP);
  }

  lastCelsius = t;
  gotReading = true;
}

bool Temperature::hasReading(){ return gotReading; }
float Temperature::celsius(){ return lastCelsius; }
bool Temperature::sensorFaulted(){ return faulted; }
