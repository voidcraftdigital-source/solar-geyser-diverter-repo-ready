#pragma once
#include <Arduino.h>

// Thin wrapper over NVS-backed Preferences for the values that must survive
// a reboot/power loss — Specification §11: Wi-Fi credentials and device name
// must be installer-configurable and persistent, not hardcoded in firmware.
namespace Settings {
  void begin();

  String wifiSsid();
  String wifiPass();
  String deviceName();

  void setWifiCredentials(const String &ssid, const String &pass);
  void setDeviceName(const String &name);
  void clearWifiCredentials();
  bool hasWifiCredentials();
}
