#pragma once
#include <Arduino.h>

// Wi-Fi connection state machine — Specification §11.
//
// WifiManager::loop() must be called every main-loop iteration and must
// never block. A connection attempt that hangs or fails is handled by a
// millis()-based timeout, never delay() — so it can never starve the
// watchdog or the heartbeat, and a bad Wi-Fi network can never itself
// cause an unexpected reset (Specification §17/§18: the watchdog resets
// on unresponsive *firmware*, not on unresponsive Wi-Fi).
namespace WifiManager {
  enum class State { CONNECTING, CONNECTED, AP_MODE };

  void begin();
  void loop();

  State state();
  String statusText();
  String ipAddress();
  String apSsid();

  // Called by the web UI after new credentials are saved — re-enters the
  // CONNECTING state on the new network rather than waiting for a reboot.
  void reconnect();
}
