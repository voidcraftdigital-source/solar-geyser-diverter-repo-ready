#include "inverter_link.h"
#include "null_inverter.h"
#include "rs485_bus.h"
#include <Arduino.h>

namespace {
  NullInverter nullImpl;
  InverterInterface *active = &nullImpl;
  constexpr uint32_t POLL_PERIOD_MS = 2000;
  uint32_t lastPoll = 0;
}

void InverterLink::begin(){
  RS485Bus::begin();
  Serial.println("[INVERTER] RS-485 bus initialized. No protocol selected yet -- using NullInverter stand-in.");
}

void InverterLink::loop(){
  uint32_t now = millis();
  if (now - lastPoll < POLL_PERIOD_MS) return;
  lastPoll = now;
  active->poll();
}

const InverterInterface &InverterLink::inverter(){ return *active; }
