#pragma once

// Boot sequencing, safe-default output init, and the hardware watchdog.
// Specification §17 (watchdog) and §25 (safe output defaults on boot).

namespace System {
  void begin();
  void loop();
}
