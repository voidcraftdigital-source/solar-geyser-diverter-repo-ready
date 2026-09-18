#include "power_control.h"
#include "pins.h"
#include "faults.h"

namespace {
  // Bench-test safety timeout only — not a production heating-cycle value,
  // this stage has no concept of a target temperature or a real control loop.
  constexpr uint32_t AUTO_OFF_MS = 10000;

  bool on = false;
  uint32_t onSince = 0;
}

void PowerControl::begin(){
  pinMode(Pins::SSR_ENABLE, OUTPUT);
  digitalWrite(Pins::SSR_ENABLE, LOW); // safe default — Specification §25
}

bool PowerControl::requestOn(){
  if (Faults::active()){
    Serial.println("[POWER] Refused SSR on: a fault is active");
    return false;
  }
  on = true;
  onSince = millis();
  digitalWrite(Pins::SSR_ENABLE, HIGH);
  Serial.println("[POWER] SSR_ENABLE on (manual test, auto-off in 10s)");
  return true;
}

void PowerControl::requestOff(){
  if (!on) return;
  on = false;
  digitalWrite(Pins::SSR_ENABLE, LOW);
  Serial.println("[POWER] SSR_ENABLE off");
}

void PowerControl::loop(){
  if (!on) return;
  if (Faults::active()){
    Serial.println("[POWER] Fault raised while SSR on -> forcing off");
    requestOff();
    return;
  }
  if (millis() - onSince >= AUTO_OFF_MS){
    Serial.println("[POWER] Auto-off timeout reached");
    requestOff();
  }
}

bool PowerControl::isOn(){ return on; }

uint32_t PowerControl::secondsRemaining(){
  if (!on) return 0;
  uint32_t elapsed = millis() - onSince;
  if (elapsed >= AUTO_OFF_MS) return 0;
  return (AUTO_OFF_MS - elapsed) / 1000;
}
