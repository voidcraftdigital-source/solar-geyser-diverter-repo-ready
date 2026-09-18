#include "faults.h"
#include "pins.h"
#include <Arduino.h>

namespace {
  bool sourceActive[(size_t)Faults::Source::COUNT] = {};

  const char *sourceName(Faults::Source s){
    switch (s){
      case Faults::Source::WATCHDOG_TEST: return "watchdog-test";
      case Faults::Source::SENSOR_TEMP:   return "sensor-temp";
      case Faults::Source::OVER_CURRENT:  return "over-current";
      default: return "unknown";
    }
  }

  bool anyActive(){
    for (bool b : sourceActive) if (b) return true;
    return false;
  }
}

void Faults::begin(){
  pinMode(Pins::LED_FAULT, OUTPUT);
  digitalWrite(Pins::LED_FAULT, LOW);
}

void Faults::raise(Source source, const char *reason){
  bool wasActive = sourceActive[(size_t)source];
  sourceActive[(size_t)source] = true;
  digitalWrite(Pins::LED_FAULT, HIGH);
  if (!wasActive){
    Serial.printf("[FAULT] %s: %s\n", sourceName(source), reason);
  }
}

void Faults::clear(Source source){
  if (!sourceActive[(size_t)source]) return;
  sourceActive[(size_t)source] = false;
  Serial.printf("[FAULT] %s: cleared\n", sourceName(source));
  if (!anyActive()){
    digitalWrite(Pins::LED_FAULT, LOW);
  }
}

bool Faults::active(){ return anyActive(); }
bool Faults::active(Source source){ return sourceActive[(size_t)source]; }
