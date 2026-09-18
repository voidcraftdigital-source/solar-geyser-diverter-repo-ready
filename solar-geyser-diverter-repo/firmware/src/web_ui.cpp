#include "web_ui.h"
#include "wifi_manager.h"
#include "settings.h"
#include "faults.h"
#include "temperature.h"
#include "current_sense.h"
#include "power_control.h"
#include "inverter_link.h"
#include "rs485_bus.h"
#include <WebServer.h>

namespace {
  WebServer httpServer(80);
  uint32_t restartAtMs = 0;
  bool restartPending = false;
  uint32_t reconnectAtMs = 0;
  bool reconnectPending = false;

  String htmlEscape(const String &in){
    String out; out.reserve(in.length());
    for (size_t i = 0; i < in.length(); i++){
      char c = in[i];
      switch (c){
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += c;
      }
    }
    return out;
  }

  String pageShell(const String &title, const String &body){
    String html;
    html.reserve(1200 + body.length());
    html += "<!doctype html><html><head><meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>" + htmlEscape(title) + "</title><style>";
    html += "body{font-family:-apple-system,system-ui,sans-serif;max-width:480px;margin:32px auto;padding:0 16px;"
            "color:#16211c;background:#eef1ec}"
            "h1{font-size:1.3rem;margin:0 0 4px}"
            "p.tag{color:#55655c;font-size:.85rem;margin:0 0 24px}"
            "table{width:100%;border-collapse:collapse;margin-bottom:20px}"
            "td{padding:8px 0;border-bottom:1px solid #d8dfd4;font-size:.92rem}"
            "td.k{color:#55655c;width:40%}"
            "td.v{font-family:ui-monospace,monospace;font-weight:600}"
            "a.btn,button{display:inline-block;background:#16211c;color:#eef1ec;text-decoration:none;"
            "padding:10px 16px;border-radius:8px;border:0;font-size:.9rem;cursor:pointer;margin-top:6px}"
            "form{margin-top:8px}label{display:block;font-size:.8rem;color:#55655c;margin:14px 0 4px}"
            "input{width:100%;box-sizing:border-box;padding:9px 10px;border-radius:7px;border:1px solid #d8dfd4;font-size:.95rem}"
            "footer{margin-top:28px;color:#8b9a8f;font-size:.75rem}"
            ".warn{background:#fbe9e5;border:1px solid #e8b3a6;border-radius:8px;padding:12px 14px;font-size:.85rem;margin-bottom:20px}"
            "button.danger{background:#d03b3b}button.off{background:#55655c}";
    html += "</style></head><body>";
    html += body;
    html += "<footer>Rev A4 Geyser Diverter &mdash; firmware V0.6</footer>";
    html += "</body></html>";
    return html;
  }

  void handleRoot(){
    String body = "<h1>" + htmlEscape(Settings::deviceName()) + "</h1>";
    body += "<p class='tag'>Local status &mdash; Specification &sect;12</p>";
    body += "<table>";
    body += "<tr><td class='k'>Wi-Fi</td><td class='v'>" + WifiManager::statusText() + "</td></tr>";
    if (WifiManager::state() == WifiManager::State::AP_MODE){
      body += "<tr><td class='k'>Setup network</td><td class='v'>" + WifiManager::apSsid() + "</td></tr>";
    }
    body += "<tr><td class='k'>Address</td><td class='v'>" + WifiManager::ipAddress() + "</td></tr>";
    body += "<tr><td class='k'>Uptime</td><td class='v'>" + String(millis() / 1000) + "s</td></tr>";
    body += "<tr><td class='k'>Fault status</td><td class='v'>" + String(Faults::active() ? "FAULT" : "OK") + "</td></tr>";
    if (Temperature::hasReading()){
      body += "<tr><td class='k'>Geyser temperature</td><td class='v'>" + String(Temperature::celsius(), 1) + "&deg;C</td></tr>";
    } else {
      body += "<tr><td class='k'>Geyser temperature</td><td class='v'>" +
        String(Temperature::sensorFaulted() ? "sensor fault" : "no reading yet") + "</td></tr>";
    }
    if (CurrentSense::hasReading()){
      body += "<tr><td class='k'>Mains voltage</td><td class='v'>" + String(CurrentSense::mainsVolts(), 0) + "V</td></tr>";
      body += "<tr><td class='k'>Grid current</td><td class='v'>" + String(CurrentSense::gridAmps(), 2) + "A</td></tr>";
      body += "<tr><td class='k'>Inverter current</td><td class='v'>" + String(CurrentSense::inverterAmps(), 2) + "A</td></tr>";
      body += "<tr><td class='k'>Geyser current</td><td class='v'>" + String(CurrentSense::geyserAmps(), 2) + "A</td></tr>";
      body += "<tr><td class='k'>Geyser power (est.)</td><td class='v'>" + String(CurrentSense::geyserPowerEstimateW(), 0) + "W</td></tr>";
    } else {
      body += "<tr><td class='k'>Current/voltage sensing</td><td class='v'>no reading yet</td></tr>";
    }
    body += "<tr><td class='k'>SSR (manual test)</td><td class='v'>" +
      (PowerControl::isOn() ? "ON, " + String(PowerControl::secondsRemaining()) + "s left" : String("off")) + "</td></tr>";
    body += "<tr><td class='k'>Inverter link</td><td class='v'>" + String(InverterLink::inverter().getInverterStatus()) + "</td></tr>";
    body += "</table>";
    body += "<a class='btn' href='/wifi'>Wi-Fi &amp; device settings</a> ";
    body += "<a class='btn' href='/power' style='background:#d03b3b'>Manual SSR test</a> ";
    body += "<a class='btn' href='/inverter'>Inverter link</a>";
    httpServer.send(200, "text/html", pageShell(Settings::deviceName(), body));
  }

  void handleWifiForm(){
    String body = "<h1>Wi-Fi &amp; device settings</h1>";
    body += "<p class='tag'>Specification &sect;11 &mdash; saved settings persist across power loss</p>";
    if (WifiManager::state() == WifiManager::State::AP_MODE){
      body += "<p class='tag'>Currently in setup mode &mdash; not connected to a home network yet.</p>";
    }
    body += "<form method='POST' action='/wifi/save'>";
    body += "<label>Wi-Fi network name (SSID)</label>";
    body += "<input name='ssid' value='" + htmlEscape(Settings::wifiSsid()) + "' required>";
    body += "<label>Wi-Fi password</label>";
    body += "<input name='pass' type='password' value='" + htmlEscape(Settings::wifiPass()) + "'>";
    body += "<label>Device name</label>";
    body += "<input name='name' value='" + htmlEscape(Settings::deviceName()) + "' required>";
    body += "<button type='submit'>Save &amp; reconnect</button>";
    body += "</form>";
    body += "<form method='POST' action='/wifi/reset' onsubmit=\"return confirm('Clear saved Wi-Fi and restart in setup mode?')\">";
    body += "<button type='submit' style='background:#d03b3b'>Forget Wi-Fi</button>";
    body += "</form>";
    body += "<p><a href='/'>&larr; back</a></p>";
    httpServer.send(200, "text/html", pageShell("Wi-Fi settings", body));
  }

  void handleWifiSave(){
    if (!httpServer.hasArg("ssid") || httpServer.arg("ssid").length() == 0){
      httpServer.send(400, "text/plain", "SSID is required");
      return;
    }
    Settings::setWifiCredentials(httpServer.arg("ssid"), httpServer.arg("pass"));
    if (httpServer.hasArg("name") && httpServer.arg("name").length() > 0){
      Settings::setDeviceName(httpServer.arg("name"));
    }
    String body = "<h1>Saved</h1><p class='tag'>Reconnecting to '" + htmlEscape(httpServer.arg("ssid")) + "' now&hellip;</p>";
    body += "<p>This page will stop responding while it switches networks. Rejoin your normal Wi-Fi and "
            "revisit the device's new address once connected.</p>";
    httpServer.send(200, "text/html", pageShell("Saved", body));
    // Deferred, not called here directly: if we're currently in AP mode, switching
    // to STA tears down the SoftAP immediately, which can cut the response off
    // mid-flight before the client's browser has actually received it. Give the
    // TCP stack a moment to flush first.
    reconnectPending = true;
    reconnectAtMs = millis() + 1000;
  }

  void handleWifiReset(){
    Settings::clearWifiCredentials();
    String body = "<h1>Wi-Fi forgotten</h1><p class='tag'>Restarting into setup mode&hellip;</p>";
    httpServer.send(200, "text/html", pageShell("Restarting", body));
    restartPending = true;
    restartAtMs = millis() + 1000; // let the response flush before rebooting
  }

  String powerBody(const String &note){
    String body = "<h1>Manual SSR test</h1>";
    body += "<div class='warn'><strong>Bench test only.</strong> Confirm this drives a low-risk test "
            "load (an LED, a small relay module, anything through the same isolated driver you'll use "
            "in the final design) &mdash; never the mains-connected geyser element at this stage "
            "(Specification &sect;28 V0.5). Turns off on its own after 10 seconds, and refuses to turn "
            "on at all while any fault is active.</div>";
    if (note.length()) body += "<p class='tag'>" + note + "</p>";
    body += "<table><tr><td class='k'>SSR_ENABLE</td><td class='v'>" +
      (PowerControl::isOn() ? "ON &mdash; " + String(PowerControl::secondsRemaining()) + "s left" : String("off")) +
      "</td></tr>";
    body += "<tr><td class='k'>Fault status</td><td class='v'>" + String(Faults::active() ? "FAULT" : "OK") + "</td></tr></table>";
    body += "<form method='POST' action='/power/on' style='display:inline'><button>Turn ON (10s test)</button></form> ";
    body += "<form method='POST' action='/power/off' style='display:inline'><button class='off' type='submit'>Turn OFF now</button></form>";
    body += "<p><a href='/'>&larr; back</a></p>";
    return body;
  }

  void handlePowerPage(){
    httpServer.send(200, "text/html", pageShell("Manual SSR test", powerBody("")));
  }

  void handlePowerOn(){
    bool ok = PowerControl::requestOn();
    String note = ok ? "Turned on." : "Refused: a fault is currently active.";
    httpServer.send(200, "text/html", pageShell("Manual SSR test", powerBody(note)));
  }

  void handlePowerOff(){
    PowerControl::requestOff();
    httpServer.send(200, "text/html", pageShell("Manual SSR test", powerBody("Turned off.")));
  }

  String inverterBody(const String &note){
    const InverterInterface &inv = InverterLink::inverter();
    String body = "<h1>Inverter link</h1>";
    body += "<p class='tag'>Specification &sect;10</p>";
    body += "<table>";
    body += "<tr><td class='k'>Status</td><td class='v'>" + String(inv.getInverterStatus()) + "</td></tr>";
    body += "<tr><td class='k'>Online</td><td class='v'>" + String(inv.isOnline() ? "yes" : "no") + "</td></tr>";
    body += "</table>";
    body += "<div class='warn'>No real inverter protocol is implemented yet &mdash; different inverter "
            "brands use incompatible RS-485 register maps, so this stage only proves the transport "
            "(UART + DE/RE turnaround) works, not communication with any actual inverter. To bench-test "
            "it, jumper GPIO17 (TX) directly to GPIO18 (RX) &mdash; or loop the transceiver's A/B lines "
            "back to themselves &mdash; then run the test below.</div>";
    if (note.length()) body += "<p class='tag'>" + note + "</p>";
    body += "<form method='POST' action='/inverter/loopback'><button>Run RS-485 loopback test</button></form>";
    body += "<p><a href='/'>&larr; back</a></p>";
    return body;
  }

  void handleInverterPage(){
    httpServer.send(200, "text/html", pageShell("Inverter link", inverterBody("")));
  }

  void handleInverterLoopback(){
    String result = RS485Bus::runLoopbackTest();
    httpServer.send(200, "text/html", pageShell("Inverter link", inverterBody(result)));
  }

  void handleApiStatus(){
    String json = "{";
    json += "\"device_name\":\"" + Settings::deviceName() + "\",";
    json += "\"firmware_stage\":\"V0.6\",";
    json += "\"wifi_status\":\"" + WifiManager::statusText() + "\",";
    json += "\"ip\":\"" + WifiManager::ipAddress() + "\",";
    json += "\"uptime_s\":" + String(millis() / 1000) + ",";
    json += "\"fault_active\":" + String(Faults::active() ? "true" : "false") + ",";
    json += "\"temperature_c\":" + (Temperature::hasReading() ? String(Temperature::celsius(), 2) : String("null")) + ",";
    json += "\"temperature_installed\":true,";
    json += "\"current_voltage_installed\":" + String(CurrentSense::hasReading() ? "true" : "false") + ",";
    json += "\"grid_a\":" + (CurrentSense::hasReading() ? String(CurrentSense::gridAmps(), 3) : String("null")) + ",";
    json += "\"inverter_a\":" + (CurrentSense::hasReading() ? String(CurrentSense::inverterAmps(), 3) : String("null")) + ",";
    json += "\"geyser_a\":" + (CurrentSense::hasReading() ? String(CurrentSense::geyserAmps(), 3) : String("null")) + ",";
    json += "\"mains_v\":" + (CurrentSense::hasReading() ? String(CurrentSense::mainsVolts(), 1) : String("null")) + ",";
    json += "\"geyser_power_est_w\":" + (CurrentSense::hasReading() ? String(CurrentSense::geyserPowerEstimateW(), 1) : String("null")) + ",";
    json += "\"ssr_on\":" + String(PowerControl::isOn() ? "true" : "false") + ",";
    json += "\"ssr_seconds_remaining\":" + String(PowerControl::secondsRemaining()) + ",";
    json += "\"inverter_online\":" + String(InverterLink::inverter().isOnline() ? "true" : "false") + ",";
    json += "\"inverter_status\":\"" + String(InverterLink::inverter().getInverterStatus()) + "\"";
    json += "}";
    httpServer.send(200, "application/json", json);
  }

  void handleNotFound(){
    httpServer.send(404, "text/plain", "Not found");
  }
}

void WebUI::begin(){
  httpServer.on("/", HTTP_GET, handleRoot);
  httpServer.on("/wifi", HTTP_GET, handleWifiForm);
  httpServer.on("/wifi/save", HTTP_POST, handleWifiSave);
  httpServer.on("/wifi/reset", HTTP_POST, handleWifiReset);
  httpServer.on("/power", HTTP_GET, handlePowerPage);
  httpServer.on("/power/on", HTTP_POST, handlePowerOn);
  httpServer.on("/power/off", HTTP_POST, handlePowerOff);
  httpServer.on("/inverter", HTTP_GET, handleInverterPage);
  httpServer.on("/inverter/loopback", HTTP_POST, handleInverterLoopback);
  httpServer.on("/api/status", HTTP_GET, handleApiStatus);
  httpServer.onNotFound(handleNotFound);
  httpServer.begin();
  Serial.println("[WEBUI] Local web server started on port 80");
}

void WebUI::loop(){
  httpServer.handleClient();
  if (restartPending && millis() >= restartAtMs){
    ESP.restart();
  }
  if (reconnectPending && millis() >= reconnectAtMs){
    reconnectPending = false;
    WifiManager::reconnect();
  }
}
