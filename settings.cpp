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

String Settings::cloudBackendUrl(){ return prefs.getString("cloud_url", ""); }
String Settings::cloudDeviceId(){ return prefs.getString("cloud_id", ""); }
String Settings::cloudDeviceSecret(){ return prefs.getString("cloud_secret", ""); }

void Settings::setCloudCredentials(const String &backendUrl, const String &deviceId, const String &deviceSecret){
  prefs.putString("cloud_url", backendUrl);
  prefs.putString("cloud_id", deviceId);
  prefs.putString("cloud_secret", deviceSecret);
}

void Settings::clearCloudCredentials(){
  prefs.remove("cloud_url");
  prefs.remove("cloud_id");
  prefs.remove("cloud_secret");
}

bool Settings::hasCloudCredentials(){
  return cloudBackendUrl().length() > 0 && cloudDeviceId().length() > 0 && cloudDeviceSecret().length() > 0;
}
