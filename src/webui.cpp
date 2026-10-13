// webui.cpp -- Wi-Fi access point + captive portal + settings page.
// Runs on core 0 in its own task so the eye animation on core 1 never stalls.
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "settings.h"
#include "puppet.h"
#include "webui.h"

static WebServer server(80);
static DNSServer dns;
static const IPAddress apIP(192, 168, 4, 1);
static String ssid;
static volatile uint32_t saveAtMs = 0;   // debounce flash writes

extern const char INDEX_HTML[] PROGMEM;

static String hex6(uint32_t c) { char b[8]; snprintf(b, sizeof(b), "#%06X", (unsigned)(c & 0xFFFFFF)); return b; }
static uint32_t parseHex(const String &s) { return strtoul(s.c_str() + (s.startsWith("#") ? 1 : 0), nullptr, 16) & 0xFFFFFF; }

static void sendSettings() {
  char b[420];
  snprintf(b, sizeof(b),
    "{\"eyeStyle\":%u,\"irisMode\":%u,\"irisColor\":\"%s\",\"eyelidColor\":\"%s\",\"brightness\":%u,"
    "\"autoBlink\":%s,\"volume\":%u,\"wakeDistMm\":%u,\"leanDistMm\":%u,\"sleepAfterS\":%u,"
    "\"flipGazeX\":%s,\"flipGazeY\":%s,\"ssid\":\"%s\"}",
    gSet.eyeStyle, gSet.irisMode, hex6(gSet.irisColor).c_str(), hex6(gSet.eyelidColor).c_str(), gSet.brightness,
    gSet.autoBlink ? "true" : "false", gSet.volume, gSet.wakeDistMm, gSet.leanDistMm, gSet.sleepAfterS,
    gSet.flipGazeX ? "true" : "false", gSet.flipGazeY ? "true" : "false", ssid.c_str());
  server.send(200, "application/json", b);
}

// POST /api/set  key=value (form encoded, any subset of keys)
static void handleSet() {
  bool volChanged = false;
  for (int i = 0; i < server.args(); i++) {
    const String k = server.argName(i), v = server.arg(i);
    long n = v.toInt();
    bool on = (v == "1" || v == "true" || v == "on");
    if      (k == "eyeStyle")    gSet.eyeStyle    = constrain(n, 0, 1);
    else if (k == "irisMode")    gSet.irisMode    = constrain(n, 0, 1);
    else if (k == "irisColor")   gSet.irisColor   = parseHex(v);
    else if (k == "eyelidColor") gSet.eyelidColor = parseHex(v);
    else if (k == "brightness")  gSet.brightness  = constrain(n, 1, 15);
    else if (k == "autoBlink")   gSet.autoBlink   = on;
    else if (k == "volume")      { gSet.volume    = constrain(n, 0, 30); volChanged = true; }
    else if (k == "wakeDistMm")  gSet.wakeDistMm  = constrain(n, 500, 6000);
    else if (k == "leanDistMm")  gSet.leanDistMm  = constrain(n, 150, 1500);
    else if (k == "sleepAfterS") gSet.sleepAfterS = constrain(n, 2, 120);
    else if (k == "flipGazeX")   gSet.flipGazeX   = on;
    else if (k == "flipGazeY")   gSet.flipGazeY   = on;
    else if (k == "wifiPass" && v.length() >= 8 && v.length() < sizeof(gSet.wifiPass))
      strlcpy(gSet.wifiPass, v.c_str(), sizeof(gSet.wifiPass));
  }
  if (volChanged) puppetApplyVolume();
  gSettingsDirty = true;
  saveAtMs = millis() + 2000;
  sendSettings();
}

static void handleCmd() { puppetCommand(server.arg("c")); server.send(200, "application/json", "{\"ok\":true}"); }

static void handleReset() {
  char pass[33]; strlcpy(pass, gSet.wifiPass, sizeof(pass));
  settingsReset();
  strlcpy(gSet.wifiPass, pass, sizeof(gSet.wifiPass));   // don't lock the user out
  puppetApplyVolume();
  gSettingsDirty = true; saveAtMs = millis() + 100;
  sendSettings();
}

static void redirectToPortal() {
  server.sendHeader("Location", String("http://") + apIP.toString() + "/", true);
  server.send(302, "text/plain", "");
}

static void webTask(void *) {
  for (;;) {
    dns.processNextRequest();
    server.handleClient();
    if (saveAtMs && (int32_t)(millis() - saveAtMs) > 0) { saveAtMs = 0; settingsSave(); Serial.println("[web] settings saved"); }
    vTaskDelay(2);
  }
}

void webuiStart() {
  // Bring the radio up in AP mode first and configure it before anything else,
  // otherwise the driver briefly advertises its default open "ESP_xxxxxx" network.
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  uint8_t mac[6]; WiFi.softAPmacAddress(mac);
  char name[24]; snprintf(name, sizeof(name), "CreeperEyes-%02X%02X", mac[4], mac[5]);
  ssid = name;
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid.c_str(), gSet.wifiPass);
  dns.start(53, "*", apIP);

  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/api/settings", HTTP_GET, sendSettings);
  server.on("/api/set", HTTP_POST, handleSet);
  server.on("/api/cmd", HTTP_POST, handleCmd);
  server.on("/api/reset", HTTP_POST, handleReset);
  server.on("/api/status", HTTP_GET, [] { server.send(200, "application/json", puppetStatusJson()); });
  server.onNotFound(redirectToPortal);   // makes phones pop the page up automatically
  server.begin();

  xTaskCreatePinnedToCore(webTask, "web", 6144, nullptr, 1, nullptr, 0);
  Serial.printf("[web] Wi-Fi \"%s\" password \"%s\" -> http://%s/\n", ssid.c_str(), gSet.wifiPass, apIP.toString().c_str());
}
