#include "settings.h"
#include <Preferences.h>

Settings gSet;
volatile bool gSettingsDirty = true;   // apply once at boot

static Preferences prefs;
static const char *NS = "creeper";
static const uint16_t kVersion = 2;    // bump if the struct layout changes

// Layout of version 1 (single look for both eyes), so old saves carry over.
struct SettingsV1 {
  uint8_t eyeStyle, irisMode; uint32_t irisColor, eyelidColor; uint8_t brightness; bool autoBlink;
  uint8_t volume; uint16_t wakeDistMm, leanDistMm, sleepAfterS; bool flipGazeX, flipGazeY; char wifiPass[33];
};

void settingsReset() {
  for (int e = 0; e < 2; e++) {
    gSet.eye[e].style       = 0;
    gSet.eye[e].irisMode    = 0;
    gSet.eye[e].irisColor   = 0x6B8E23;   // olive green
    gSet.eye[e].eyelidColor = 0x506850;   // muted Frankenstein green (= RGB565 0x534A)
  }
  gSet.linkEyes    = true;
  gSet.brightness  = 15;
  gSet.autoBlink   = true;
  gSet.volume      = 25;
  gSet.wakeDistMm  = 2500;
  gSet.leanDistMm  = 450;
  gSet.sleepAfterS = 8;
  gSet.flipGazeX   = false;
  gSet.flipGazeY   = false;
  strlcpy(gSet.wifiPass, "frankenstein", sizeof(gSet.wifiPass));
}

void settingsLoad() {
  settingsReset();
  prefs.begin(NS, true);
  uint16_t ver = prefs.getUShort("ver", 0);
  if (ver == kVersion && prefs.getBytesLength("set") == sizeof(Settings)) {
    prefs.getBytes("set", &gSet, sizeof(Settings));
  } else if (ver == 1 && prefs.getBytesLength("set") == sizeof(SettingsV1)) {
    SettingsV1 o; prefs.getBytes("set", &o, sizeof(o));
    for (int e = 0; e < 2; e++) gSet.eye[e] = {o.eyeStyle, o.irisMode, o.irisColor, o.eyelidColor};
    gSet.brightness = o.brightness; gSet.autoBlink = o.autoBlink; gSet.volume = o.volume;
    gSet.wakeDistMm = o.wakeDistMm; gSet.leanDistMm = o.leanDistMm; gSet.sleepAfterS = o.sleepAfterS;
    gSet.flipGazeX = o.flipGazeX; gSet.flipGazeY = o.flipGazeY;
    memcpy(gSet.wifiPass, o.wifiPass, sizeof(gSet.wifiPass));
  }
  prefs.end();
  gSet.wifiPass[sizeof(gSet.wifiPass) - 1] = 0;
  if (strlen(gSet.wifiPass) < 8) strlcpy(gSet.wifiPass, "frankenstein", sizeof(gSet.wifiPass));
  gSettingsDirty = true;
}

void settingsSave() {
  prefs.begin(NS, false);
  prefs.putUShort("ver", kVersion);
  prefs.putBytes("set", &gSet, sizeof(Settings));
  prefs.end();
}

uint16_t rgb888to565(uint32_t c) {
  uint8_t r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
