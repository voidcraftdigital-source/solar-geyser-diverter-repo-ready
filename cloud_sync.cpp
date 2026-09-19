#include "cloud_sync.h"
#include "cloud_ca_cert.h"
#include "settings.h"
#include "wifi_manager.h"
#include "faults.h"
#include "temperature.h"
#include "current_sense.h"
#include "power_control.h"
#include "inverter_link.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
  constexpr uint32_t PUSH_INTERVAL_MS = 15000;   // bench-test cadence; tune once real deployments exist
  constexpr uint32_t RETRY_INTERVAL_MS = 5000;   // when Wi-Fi/credentials aren't ready yet
  constexpr uint32_t HTTP_TIMEOUT_MS = 10000;

  // Written only by syncTask (one producer), read only by the main loop via
  // the accessor functions below (one consumer) — a spinlock around every
  // access is cheap and removes any need to reason about tearing on the
  // multi-byte fields (uint32_t, String) across the two cores.
  portMUX_TYPE statusMux = portMUX_INITIALIZER_UNLOCKED;
  bool everSyncedFlag = false;
  bool lastOkFlag = false;
  uint32_t lastSyncMs = 0;
  String lastErrorMsg = "not synced yet";

  void setStatus(bool ok, const String &error){
    portENTER_CRITICAL(&statusMux);
    everSyncedFlag = true;
    lastOkFlag = ok;
    lastSyncMs = millis();
    lastErrorMsg = error;
    portEXIT_CRITICAL(&statusMux);
  }

  // Hand-built JSON, matching the style already used in web_ui.cpp's
  // handleApiStatus() rather than pulling in ArduinoJson for one small
  // object. Only fields this firmware can actually measure are populated
  // with real numbers; everything the automatic control algorithm (V0.9,
  // not built yet) or a real inverter link (no protocol implemented yet)
  // would supply is sent as null rather than a made-up value — Specification
  // §10's "safe fallback... not assumptions about available PV" applies
  // just as much to what this device tells the cloud as to its own control
  // logic.
  String buildTelemetryPayload(){
    String json = "{";
    json += "\"firmware_stage\":\"V0.10\",";
    json += "\"uptime_s\":" + String(millis() / 1000) + ",";
    json += "\"mode\":\"manual-test\","; // no automatic mode selection exists yet (V0.9)
    json += "\"heating\":" + String(PowerControl::isOn() ? "true" : "false") + ",";
    json += "\"fault_active\":" + String(Faults::active() ? "true" : "false") + ",";
    json += "\"fault_reason\":" + (Faults::active() ? ("\"" + String(Faults::activeSourceName()) + "\"") : String("null")) + ",";
    json += "\"temperature_c\":" + (Temperature::hasReading() ? String(Temperature::celsius(), 2) : String("null")) + ",";
    json += "\"target_c\":null,"; // set by the app/backend, not this firmware — nothing to report yet
    json += "\"geyser_current_a\":" + (CurrentSense::hasReading() ? String(CurrentSense::geyserAmps(), 3) : String("null")) + ",";
    json += "\"geyser_power_w\":" + (CurrentSense::hasReading() ? String(CurrentSense::geyserPowerEstimateW(), 1) : String("null")) + ",";
    json += "\"grid_a\":" + (CurrentSense::hasReading() ? String(CurrentSense::gridAmps(), 3) : String("null")) + ",";
    json += "\"inverter_a\":" + (CurrentSense::hasReading() ? String(CurrentSense::inverterAmps(), 3) : String("null")) + ",";
    json += "\"mains_v\":" + (CurrentSense::hasReading() ? String(CurrentSense::mainsVolts(), 1) : String("null")) + ",";

    const InverterInterface &inv = InverterLink::inverter();
    bool invOnline = inv.isOnline();
    json += "\"inverter_online\":" + String(invOnline ? "true" : "false") + ",";
    json += "\"inverter_status\":\"" + String(inv.getInverterStatus()) + "\",";
    json += "\"pv_kw\":" + (invOnline ? String(inv.getPVPower() / 1000.0f, 3) : String("null")) + ",";
    json += "\"house_kw\":" + (invOnline ? String(inv.getHouseLoad() / 1000.0f, 3) : String("null")) + ",";
    json += "\"battery_soc_pct\":" + (invOnline ? String(inv.getBatterySOC(), 1) : String("null")) + ",";
    json += "\"surplus_w\":null"; // needs the surplus algorithm (V0.7+), not built yet
    json += "}";
    return json;
  }

  void syncOnce(){
    WiFiClientSecure client;
    client.setCACert(CLOUD_CA_CERT);

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    String url = Settings::cloudBackendUrl() + "/api/device/telemetry";
    if (!http.begin(client, url)){
      setStatus(false, "http.begin() failed — check the backend URL");
      return;
    }
    http.addHeader("Content-Type", "application/json");
    http.setAuthorization(Settings::cloudDeviceId().c_str(), Settings::cloudDeviceSecret().c_str());

    int status = http.POST(buildTelemetryPayload());
    if (status <= 0){
      setStatus(false, "connection failed: " + http.errorToString(status));
      http.end();
      return;
    }
    if (status != 200){
      String body = http.getString();
      setStatus(false, "backend returned " + String(status) + ": " + body.substring(0, 80));
      http.end();
      return;
    }

    String body = http.getString();
    http.end();
    setStatus(true, "");

    // Command write path deliberately not built yet — same gap as the web
    // app's test client (../webapp/README.md). Logged so a pending command
    // is visible on the serial console rather than silently dropped, never
    // applied or falsely acked as if it had been.
    if (body.indexOf("\"commands\":[]") == -1 && body.indexOf("\"commands\":null") == -1){
      Serial.println("[CLOUD] backend has pending command(s) for this device — not applied (write path not built yet):");
      Serial.println(body);
    }
  }

  void syncTask(void *){
    // Give Wi-Fi a moment to come up before the first attempt rather than
    // immediately failing and logging noise on every boot.
    vTaskDelay(pdMS_TO_TICKS(3000));
    for (;;){
      if (!Settings::hasCloudCredentials()){
        setStatus(false, "no cloud credentials saved — set them at /cloud");
        vTaskDelay(pdMS_TO_TICKS(RETRY_INTERVAL_MS));
        continue;
      }
      if (WifiManager::state() != WifiManager::State::CONNECTED){
        setStatus(false, "waiting for Wi-Fi");
        vTaskDelay(pdMS_TO_TICKS(RETRY_INTERVAL_MS));
        continue;
      }
      syncOnce();
      vTaskDelay(pdMS_TO_TICKS(PUSH_INTERVAL_MS));
    }
  }
}

void CloudSync::begin(){
  // Runs for the life of the device — same lifetime as the loop task, so
  // never deleted. Stack size is generous on purpose: TLS handshakes are
  // one of the more stack-hungry things this firmware does.
  xTaskCreate(syncTask, "cloud_sync", 10240, nullptr, 1, nullptr);
}

void CloudSync::loop(){
  // Intentionally empty — all the work happens in syncTask. Kept as a
  // function (rather than omitted from main.cpp's loop()) so every module
  // follows the same begin()/loop() shape and adding real per-tick work
  // later (e.g. exposing status to WebUI) doesn't change main.cpp again.
}

bool CloudSync::hasCredentials(){ return Settings::hasCloudCredentials(); }

bool CloudSync::everSynced(){
  portENTER_CRITICAL(&statusMux);
  bool v = everSyncedFlag;
  portEXIT_CRITICAL(&statusMux);
  return v;
}

bool CloudSync::lastSyncOk(){
  portENTER_CRITICAL(&statusMux);
  bool v = lastOkFlag;
  portEXIT_CRITICAL(&statusMux);
  return v;
}

uint32_t CloudSync::lastSyncAgoMs(){
  portENTER_CRITICAL(&statusMux);
  bool synced = everSyncedFlag;
  uint32_t t = lastSyncMs;
  portEXIT_CRITICAL(&statusMux);
  if (!synced) return UINT32_MAX;
  return millis() - t;
}

String CloudSync::lastError(){
  portENTER_CRITICAL(&statusMux);
  String v = lastErrorMsg;
  portEXIT_CRITICAL(&statusMux);
  return v;
}
