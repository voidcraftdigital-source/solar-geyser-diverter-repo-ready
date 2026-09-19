#pragma once
#include <Arduino.h>

// Cloud telemetry push — Specification §13 / prototype build stage V0.10.
// Pushes real sensor/state readings to the cloud backend
// (../../backend/README.md) on a timer, and logs any commands the backend
// sends back — it does NOT apply them yet; see loop()'s doc comment.
//
// Runs its own FreeRTOS task instead of doing HTTPS work from the main
// loop. A TLS handshake + request can easily take longer than the 5s
// watchdog timeout (System::begin()) on a slow or congested network, and
// nothing about a slow cloud connection should ever be able to reset the
// board — same non-blocking discipline as WifiManager, just enforced with
// a separate task instead of a millis() state machine, because unlike
// Wi-Fi connect there's no non-blocking HTTPS client available in the
// Arduino core to poll instead.
namespace CloudSync {
  void begin();
  void loop(); // cheap — only touches a status snapshot, never blocks

  bool hasCredentials();
  bool everSynced();
  bool lastSyncOk();
  uint32_t lastSyncAgoMs(); // huge number if never synced
  String lastError();       // empty if lastSyncOk()
}
