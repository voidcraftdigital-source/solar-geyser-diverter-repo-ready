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

  // Cloud backend identity — Specification §13. device_id/device_secret are
  // issued once by the backend's manufacturing endpoint and never change;
  // they are NOT the human-facing claim code, which is single-use and
  // consumed at pairing time. See ../../backend/README.md's two-credential
  // design. There is no factory provisioning tool yet (§13's flagged gap),
  // so for now these are entered by hand on the /cloud setup page using
  // credentials obtained by calling the manufacturing endpoint directly.
  String cloudBackendUrl();
  String cloudDeviceId();
  String cloudDeviceSecret();

  void setCloudCredentials(const String &backendUrl, const String &deviceId, const String &deviceSecret);
  void clearCloudCredentials();
  bool hasCloudCredentials();
}
