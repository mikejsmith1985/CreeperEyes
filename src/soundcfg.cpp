#include "soundcfg.h"
#include <Preferences.h>
#include <ArduinoJson.h>

static SoundCfg cfg;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;   // web task writes, render thread reads
static const char *kRoleKeys[R_COUNT] = {"wake", "growl", "lean", "roar", "snore", "hmm", "chuckle"};
static const uint8_t kVer = 1;

static void defaults(SoundCfg &c) {
  memset(&c, 0, sizeof(c));
  const struct { const char *n; uint16_t t; } b[] = {
    {"Groan", 1}, {"Growl 1", 2}, {"Growl 2", 3}, {"Growl 3", 4}, {"Moan", 5}, {"Mmm...", 6},
    {"Roar", 7}, {"Snore", 8}, {"Hmm?", 9}, {"Chuckle", 10}};
  c.nButtons = sizeof(b) / sizeof(b[0]);
  for (int i = 0; i < c.nButtons; i++) { strlcpy(c.buttons[i].name, b[i].n, sizeof(c.buttons[i].name)); c.buttons[i].track = b[i].t; }
  auto set = [&](SoundRole r, std::initializer_list<uint16_t> l) { c.nRole[r] = 0; for (auto t : l) c.role[r][c.nRole[r]++] = t; };
  set(R_WAKE, {1}); set(R_GROWL, {2, 3, 4, 5}); set(R_LEAN, {6}); set(R_ROAR, {7});
  set(R_SNORE, {8}); set(R_HMM, {9}); set(R_CHUCKLE, {10});
}

static void save() {
  Preferences p; p.begin("creepers", false); p.putUChar("ver", kVer); p.putBytes("cfg", &cfg, sizeof(cfg)); p.end();
}

void soundCfgLoad() {
  defaults(cfg);
  Preferences p; p.begin("creepers", true);
  if (p.getUChar("ver", 0) == kVer && p.getBytesLength("cfg") == sizeof(cfg)) p.getBytes("cfg", &cfg, sizeof(cfg));
  p.end();
}

void soundCfgReset() { SoundCfg c; defaults(c); portENTER_CRITICAL(&mux); cfg = c; portEXIT_CRITICAL(&mux); save(); }

uint16_t soundForRole(SoundRole r) {
  uint16_t t = 0;
  portENTER_CRITICAL(&mux);
  if (cfg.nRole[r]) t = cfg.role[r][esp_random() % cfg.nRole[r]];
  portEXIT_CRITICAL(&mux);
  return t;
}

bool soundRoleIsDefault(SoundRole r) {
  SoundCfg d; defaults(d);
  portENTER_CRITICAL(&mux);
  bool same = cfg.nRole[r] == d.nRole[r] && !memcmp(cfg.role[r], d.role[r], sizeof(uint16_t) * d.nRole[r]);
  portEXIT_CRITICAL(&mux);
  return same;
}

String soundCfgJson(uint16_t filesOnCard) {
  SoundCfg c; portENTER_CRITICAL(&mux); c = cfg; portEXIT_CRITICAL(&mux);
  JsonDocument d;
  d["files"] = filesOnCard;
  JsonArray b = d["buttons"].to<JsonArray>();
  for (int i = 0; i < c.nButtons; i++) { JsonObject o = b.add<JsonObject>(); o["n"] = c.buttons[i].name; o["t"] = c.buttons[i].track; }
  JsonObject ro = d["roles"].to<JsonObject>();
  for (int r = 0; r < R_COUNT; r++) { JsonArray a = ro[kRoleKeys[r]].to<JsonArray>(); for (int k = 0; k < c.nRole[r]; k++) a.add(c.role[r][k]); }
  String s; serializeJson(d, s); return s;
}

bool soundCfgFromJson(const String &json, String &err) {
  JsonDocument d;
  if (deserializeJson(d, json)) { err = "Couldn't read that sound list"; return false; }
  SoundCfg c; memset(&c, 0, sizeof(c));
  for (JsonObject o : d["buttons"].as<JsonArray>()) {
    if (c.nButtons >= SND_MAX_BUTTONS) { err = "Up to 16 buttons"; return false; }
    const char *n = o["n"] | "";
    int t = o["t"] | 0;
    if (!*n) { err = "Every button needs a name"; return false; }
    if (t < 1 || t > 2999) { err = String("\"") + n + "\" needs a file number from 1 to 2999"; return false; }
    strlcpy(c.buttons[c.nButtons].name, n, sizeof(c.buttons[0].name));
    c.buttons[c.nButtons++].track = t;
  }
  JsonObject ro = d["roles"].as<JsonObject>();
  for (int r = 0; r < R_COUNT; r++) {
    for (JsonVariant v : ro[kRoleKeys[r]].as<JsonArray>()) {
      int t = v.as<int>();
      if (t < 1 || t > 2999 || c.nRole[r] >= SND_MAX_PER_ROLE) continue;
      c.role[r][c.nRole[r]++] = t;
    }
  }
  portENTER_CRITICAL(&mux); cfg = c; portEXIT_CRITICAL(&mux);
  save();
  return true;
}
