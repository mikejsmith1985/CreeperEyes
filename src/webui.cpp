// webui.cpp -- Wi-Fi access point + captive portal + settings page.
// Runs on core 0 in its own task so the eye animation on core 1 never stalls.
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include <ESPmDNS.h>
#include "settings.h"
#include "puppet.h"
#include "webui.h"
#include "eyestyle.h"
#include "soundcfg.h"

static WebServer server(80);
static DNSServer dns;
static const IPAddress apIP(192, 168, 4, 1);
static String ssid;
static volatile uint32_t saveAtMs = 0;   // debounce flash writes
volatile bool gOtaActive = false;
static String otaError;

extern const char INDEX_HTML[] PROGMEM;
extern const uint8_t ICON_PNG[] PROGMEM;
extern const size_t ICON_PNG_LEN;

static String hex6(uint32_t c) { char b[8]; snprintf(b, sizeof(b), "#%06X", (unsigned)(c & 0xFFFFFF)); return b; }
static uint32_t parseHex(const String &s) { return strtoul(s.c_str() + (s.startsWith("#") ? 1 : 0), nullptr, 16) & 0xFFFFFF; }

static void sendSettings() {
  String j; j.reserve(1400);
  j += "{";
  const char *side[2] = {"L", "R"};
  for (int e = 0; e < 2; e++) {
    const EyeLook &L = gSet.eye[e];
    j += "\"style" + String(side[e]) + "\":" + L.style + ",";
    j += "\"irisMode" + String(side[e]) + "\":" + L.irisMode + ",";
    j += "\"irisColor" + String(side[e]) + "\":\"" + hex6(L.irisColor) + "\",";
    j += "\"eyelidColor" + String(side[e]) + "\":\"" + hex6(L.eyelidColor) + "\",";
  }
  char b[300];
  snprintf(b, sizeof(b),
    "\"linkEyes\":%s,\"brightness\":%u,\"autoBlink\":%s,\"volume\":%u,\"wakeDistMm\":%u,\"leanDistMm\":%u,"
    "\"sleepAfterS\":%u,\"flipGazeX\":%s,\"flipGazeY\":%s,\"ssid\":\"%s\",",
    gSet.linkEyes ? "true" : "false", gSet.brightness, gSet.autoBlink ? "true" : "false", gSet.volume,
    gSet.wakeDistMm, gSet.leanDistMm, gSet.sleepAfterS,
    gSet.flipGazeX ? "true" : "false", gSet.flipGazeY ? "true" : "false", ssid.c_str());
  j += b;
  snprintf(b, sizeof(b),
    "\"kidMode\":%s,\"randomEyes\":%s,\"transforms\":%s,\"jumpScares\":%s,\"antics\":%s,"
    "\"sleepSounds\":%s,\"powerSaver\":%s,\"rangeMm\":%u,\"fw\":\"%s %s\",",
    gSet.kidMode ? "true" : "false", gSet.randomEyes ? "true" : "false", gSet.transforms ? "true" : "false",
    gSet.jumpScares ? "true" : "false", gSet.antics ? "true" : "false", gSet.sleepSounds ? "true" : "false",
    gSet.powerSaver ? "true" : "false", gSet.rangeMm, __DATE__, __TIME__);
  j += b;
  j += "\"styles\":[";
  for (int i = 0; i < kEyeStyleCount; i++) { if (i) j += ","; j += "\""; j += kEyeStyles[i].name; j += "\""; }
  j += "]}";
  server.send(200, "application/json", j);
}

// POST /api/set  key=value (form encoded, any subset of keys)
// Applies one setting by name. Returns true if the volume needs pushing to the DFPlayer.
static bool setOne(const String &k, const String &v) {
    bool volChanged = false;
    long n = v.toInt();
    bool on = (v == "1" || v == "true" || v == "on");
    // per-eye keys end in L or R, e.g. styleL / irisColorR
    int e = k.endsWith("L") ? 0 : k.endsWith("R") ? 1 : -1;
    String base = e >= 0 ? k.substring(0, k.length() - 1) : k;
    if (e >= 0 && base == "style")            gSet.eye[e].style       = constrain(n, 0, kEyeStyleCount - 1);
    else if (e >= 0 && base == "irisMode")    gSet.eye[e].irisMode    = constrain(n, 0, 1);
    else if (e >= 0 && base == "irisColor")   gSet.eye[e].irisColor   = parseHex(v);
    else if (e >= 0 && base == "eyelidColor") gSet.eye[e].eyelidColor = parseHex(v);
    else if (k == "linkEyes")    gSet.linkEyes    = on;
    else if (k == "brightness")  gSet.brightness  = constrain(n, 1, 15);
    else if (k == "autoBlink")   gSet.autoBlink   = on;
    else if (k == "volume")      { gSet.volume    = constrain(n, 0, 30); volChanged = true; }
    else if (k == "wakeDistMm")  gSet.wakeDistMm  = constrain(n, 300, 6000);
    else if (k == "rangeMm")     gSet.rangeMm     = constrain(n, 500, 6000);
    else if (k == "leanDistMm")  gSet.leanDistMm  = constrain(n, 150, 1500);
    else if (k == "sleepAfterS") gSet.sleepAfterS = constrain(n, 2, 120);
    else if (k == "flipGazeX")   gSet.flipGazeX   = on;
    else if (k == "flipGazeY")   gSet.flipGazeY   = on;
    else if (k == "kidMode")     { gSet.kidMode   = on; volChanged = true; }
    else if (k == "randomEyes")  gSet.randomEyes  = on;
    else if (k == "transforms")  gSet.transforms  = on;
    else if (k == "jumpScares")  gSet.jumpScares  = on;
    else if (k == "antics")      gSet.antics      = on;
    else if (k == "sleepSounds") gSet.sleepSounds = on;
    else if (k == "powerSaver")  gSet.powerSaver  = on;
    else if (k == "wifiPass" && v.length() >= 8 && v.length() < sizeof(gSet.wifiPass))
      strlcpy(gSet.wifiPass, v.c_str(), sizeof(gSet.wifiPass));
    else return volChanged;
    if (e >= 0 || k == "linkEyes") puppetPreviewLook();
    Serial.printf("[set] %s=%s\n", k.c_str(), k == "wifiPass" ? "***" : v.c_str());
    return volChanged;
}

static void handleSet() {
  bool volChanged = false;
  for (int i = 0; i < server.args(); i++) volChanged |= setOne(server.argName(i), server.arg(i));
  if (volChanged) puppetApplyVolume();
  gSettingsDirty = true;
  saveAtMs = millis() + 2000;
  sendSettings();
}

// Serial test hook: "sstyleL=3" sets a value exactly like the phone page does.
void webuiSerialSet(const char *kv) {
  String s(kv); int eq = s.indexOf('=');
  if (eq < 1) return;
  if (setOne(s.substring(0, eq), s.substring(eq + 1))) puppetApplyVolume();
  gSettingsDirty = true; saveAtMs = millis() + 2000;
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

// POST /api/puppet  x,y (0-1023, optional) a=action (optional)
static void handlePuppet() {
  int x = server.hasArg("x") ? server.arg("x").toInt() : -1;
  int y = server.hasArg("y") ? server.arg("y").toInt() : -1;
  int a = server.hasArg("a") ? server.arg("a").toInt() : 0;
  puppetPhone(x, y, a);
  server.send(200, "application/json", "{\"ok\":true}");
}

// Wireless firmware update: POST /update with the app .bin (multipart form upload)
static void handleUpdateUpload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    otaError = "";
    gOtaActive = true;
    Serial.printf("[ota] receiving %s\n", up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) otaError = Update.errorString();
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (otaError.length()) return;
    if (up.totalSize == 0 && up.currentSize && up.buf[0] != 0xE9) {   // not an ESP32 app image
      otaError = "That file isn't CreeperEyes firmware (use creepereyes-update.bin)";
      Update.abort(); return;
    }
    if (Update.write(up.buf, up.currentSize) != up.currentSize) otaError = Update.errorString();
  } else if (up.status == UPLOAD_FILE_END) {
    if (!otaError.length() && !Update.end(true)) otaError = Update.errorString();
    Serial.printf("[ota] %u bytes, %s\n", up.totalSize, otaError.length() ? otaError.c_str() : "OK");
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort(); otaError = "Upload interrupted";
  }
}

static void handleUpdateDone() {
  if (otaError.length()) {
    gOtaActive = false;
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"" + otaError + "\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
  delay(800);
  ESP.restart();
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
    if (!gOtaActive) puppetSaveVisitsIfNeeded();
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
  server.on("/api/puppet", HTTP_POST, handlePuppet);
  server.on("/api/sounds", HTTP_GET, [] { server.send(200, "application/json", soundCfgJson(puppetFilesOnCard())); });
  server.on("/api/sounds", HTTP_POST, [] {
    String err;
    if (!soundCfgFromJson(server.arg("cfg"), err)) { server.send(400, "application/json", "{\"error\":\"" + err + "\"}"); return; }
    server.send(200, "application/json", soundCfgJson(puppetFilesOnCard()));
  });
  server.on("/api/sounds/reset", HTTP_POST, [] { soundCfgReset(); server.send(200, "application/json", soundCfgJson(puppetFilesOnCard())); });
  server.on("/api/visits/reset", HTTP_POST, [] { puppetResetVisitsTonight(); server.send(200, "application/json", puppetStatusJson()); });
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.on("/api/status", HTTP_GET, [] { server.send(200, "application/json", puppetStatusJson()); });
  // Answer the phones' "is there internet?" probes with the expected replies, so iOS/Android
  // don't show their captive-portal sheet over the page. Open http://192.168.4.1 or
  // http://creeper.local in the browser instead (or add it to the home screen).
  server.on("/hotspot-detect.html", HTTP_GET, [] {        // Apple
    server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>"); });
  server.on("/library/test/success.html", HTTP_GET, [] {
    server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>"); });
  server.on("/generate_204", HTTP_GET, [] { server.send(204); });   // Android / Chrome
  server.on("/gen_204", HTTP_GET, [] { server.send(204); });
  server.on("/connecttest.txt", HTTP_GET, [] { server.send(200, "text/plain", "Microsoft Connect Test"); });
  server.on("/ncsi.txt", HTTP_GET, [] { server.send(200, "text/plain", "Microsoft NCSI"); });
  server.on("/apple-touch-icon.png", HTTP_GET, [] { server.send_P(200, "image/png", (const char *)ICON_PNG, ICON_PNG_LEN); });
  server.onNotFound(redirectToPortal);
  server.begin();
  if (MDNS.begin("creeper")) MDNS.addService("http", "tcp", 80);   // http://creeper.local

  xTaskCreatePinnedToCore(webTask, "web", 8192, nullptr, 1, nullptr, 0);
  Serial.printf("[web] Wi-Fi \"%s\" password \"%s\" -> http://%s/ or http://creeper.local/\n", ssid.c_str(), gSet.wifiPass, apIP.toString().c_str());
}
