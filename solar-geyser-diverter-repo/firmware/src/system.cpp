#include "system.h"
#include "pins.h"
#include "faults.h"
#include <Arduino.h>
#include <esp_task_wdt.h>

namespace {
  constexpr uint32_t WATCHDOG_TIMEOUT_S = 5;
  constexpr uint32_t HEARTBEAT_PERIOD_MS = 500;

  uint32_t lastHeartbeatToggle = 0;
  bool heartbeatOn = false;
  bool watchdogTestArmed = false;
}

void System::begin(){
  // Safe default state before anything else touches a GPIO — Specification §25:
  // every output must come up LOW/off, never in whatever state the pin floats to.
  // LED_LINK is initialized here too (owned/driven from then on by WifiManager)
  // so it never floats between boot and WifiManager::begin() running.
  pinMode(Pins::LED_HEARTBEAT, OUTPUT);
  pinMode(Pins::LED_LINK, OUTPUT);
  digitalWrite(Pins::LED_HEARTBEAT, LOW);
  digitalWrite(Pins::LED_LINK, LOW);
  Faults::begin();

  Serial.begin(115200);
  delay(200); // let the USB-serial bridge settle before the first log line
  Serial.println();
  Serial.println("[BOOT] Geyser Diverter firmware V0.6 (+ RS-485 transport, no inverter protocol yet)");
  Serial.println("[BOOT] Outputs initialized to safe (LOW) default state");
  Serial.println("[BOOT] If GPIO7/8/9 have no LEDs wired yet, watch this log instead —");
  Serial.println("[BOOT] every heartbeat line below is the same signal the LED shows.");

  pinMode(Pins::WATCHDOG_TEST_PIN, INPUT_PULLUP);
  watchdogTestArmed = (digitalRead(Pins::WATCHDOG_TEST_PIN) == LOW);
  if (watchdogTestArmed){
    Serial.println("[BOOT] BOOT button held at startup — watchdog test armed.");
    Serial.println("[BOOT] The loop will deliberately hang 5s from now to prove the");
    Serial.println("[BOOT] watchdog resets the board when firmware stops responding.");
  }

  esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true /* panic + reset on timeout */);
  esp_task_wdt_add(NULL);
  Serial.printf("[BOOT] Watchdog armed: %us timeout\n", WATCHDOG_TIMEOUT_S);
}

void System::loop(){
  esp_task_wdt_reset();

  uint32_t now = millis();
  if (now - lastHeartbeatToggle >= HEARTBEAT_PERIOD_MS){
    lastHeartbeatToggle = now;
    heartbeatOn = !heartbeatOn;
    digitalWrite(Pins::LED_HEARTBEAT, heartbeatOn ? HIGH : LOW);
    Serial.printf("[HEARTBEAT] alive, uptime=%lus, fault=%s\n",
      (unsigned long)(now / 1000), Faults::active() ? "yes" : "no");
  }

  if (watchdogTestArmed && now > 5000){
    Faults::raise(Faults::Source::WATCHDOG_TEST, "intentional hang for self-test");
    Serial.println("[TEST] Hanging now — expect a reset in <=5s if the watchdog is working.");
    while (true) { /* deliberately unresponsive: stops feeding the watchdog on purpose */ }
  }
}
