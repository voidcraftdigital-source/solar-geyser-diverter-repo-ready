#pragma once

// Local web interface — Specification §12.
//
// At this stage (V0.2) it shows only what actually exists: device identity,
// Wi-Fi status, uptime, fault state, and the Wi-Fi setup form. It does not
// fabricate PV/temperature/battery data — those rows arrive with the stages
// that actually produce them (V0.3 onward).
namespace WebUI {
  void begin();
  void loop();
}
