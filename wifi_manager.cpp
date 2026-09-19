#include "wifi_manager.h"
#include "settings.h"
#include "pins.h"
#include <WiFi.h>

namespace {
  WifiManager::State currentState = WifiManager::State::AP_MODE;
  uint32_t connectStartedAt = 0;
  constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
  String apSsidValue;

  uint32_t lastLedToggle = 0;
  bool ledOn = false;

  void startAPMode(){
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char suffix[5];
    snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
    apSsidValue = String("GeyserDiverter-") + suffix;

    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSsidValue.c_str());
    currentState = WifiManager::State::AP_MODE;
    Serial.printf("[WIFI] No/failed credentials -> AP mode. SSID '%s', http://192.168.4.1/\n", apSsidValue.c_str());
  }

  void startConnecting(){
    WiFi.mode(WIFI_STA);
    WiFi.begin(Settings::wifiSsid().c_str(), Settings::wifiPass().c_str());
    connectStartedAt = millis();
    currentState = WifiManager::State::CONNECTING;
    Serial.printf("[WIFI] Connecting to '%s'...\n", Settings::wifiSsid().c_str());
  }
}

void WifiManager::begin(){
  pinMode(Pins::LED_LINK, OUTPUT);
  digitalWrite(Pins::LED_LINK, LOW);

  if (Settings::hasWifiCredentials()){
    startConnecting();
  } else {
    startAPMode();
  }
}

void WifiManager::reconnect(){
  startConnecting();
}

void WifiManager::loop(){
  uint32_t now = millis();

  switch (currentState){
    case State::CONNECTING:
      if (WiFi.status() == WL_CONNECTED){
        currentState = State::CONNECTED;
        Serial.printf("[WIFI] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
        digitalWrite(Pins::LED_LINK, HIGH);
      } else if (now - connectStartedAt > CONNECT_TIMEOUT_MS){
        Serial.println("[WIFI] Connect timed out -> falling back to AP mode.");
        startAPMode();
      } else if (now - lastLedToggle > 400){
        lastLedToggle = now; ledOn = !ledOn; digitalWrite(Pins::LED_LINK, ledOn); // slow blink while connecting
      }
      break;

    case State::AP_MODE:
      if (now - lastLedToggle > 150){
        lastLedToggle = now; ledOn = !ledOn; digitalWrite(Pins::LED_LINK, ledOn); // fast blink in setup mode
      }
      break;

    case State::CONNECTED:
      if (WiFi.status() != WL_CONNECTED){
        Serial.println("[WIFI] Connection lost -> reconnecting.");
        startConnecting();
      }
      break;
  }
}

WifiManager::State WifiManager::state(){ return currentState; }

String WifiManager::statusText(){
  switch (currentState){
    case State::CONNECTED:  return "connected";
    case State::CONNECTING: return "connecting";
    case State::AP_MODE:    return "setup (AP) mode";
  }
  return "unknown";
}

String WifiManager::ipAddress(){
  if (currentState == State::CONNECTED) return WiFi.localIP().toString();
  if (currentState == State::AP_MODE) return WiFi.softAPIP().toString();
  return "-";
}

String WifiManager::apSsid(){ return apSsidValue; }
