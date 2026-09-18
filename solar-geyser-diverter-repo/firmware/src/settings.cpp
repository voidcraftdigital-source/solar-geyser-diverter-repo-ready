#include "settings.h"
#include <Preferences.h>

namespace {
  Preferences prefs;
  const char *NAMESPACE = "geyser";
}

void Settings::begin(){
  prefs.begin(NAMESPACE, false);
}

String Settings::wifiSsid(){ return prefs.getString("wifi_ssid", ""); }
String Settings::wifiPass(){ return prefs.getString("wifi_pass", ""); }
String Settings::deviceName(){ return prefs.getString("device_name", "Geyser Diverter"); }

void Settings::setWifiCredentials(const String &ssid, const String &pass){
  prefs.putString("wifi_ssid", ssid);
  prefs.putString("wifi_pass", pass);
}

void Settings::setDeviceName(const String &name){
  prefs.putString("device_name", name);
}

void Settings::clearWifiCredentials(){
  prefs.remove("wifi_ssid");
  prefs.remove("wifi_pass");
}

bool Settings::hasWifiCredentials(){
  return wifiSsid().length() > 0;
}
