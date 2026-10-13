#include "settings.h"
#include <Preferences.h>

Settings gSet;
volatile bool gSettingsDirty = true;   // apply once at boot

static Preferences prefs;
static const char *NS = "creeper";
static const uint16_t kVersion = 1;    // bump if the struct layout changes

void settingsReset() {
  gSet.eyeStyle    = 0;
  gSet.irisMode    = 0;
  gSet.irisColor   = 0x6B8E23;         // olive green
  gSet.eyelidColor = 0x506850;         // muted Frankenstein green (= RGB565 0x534A)
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
  if (prefs.getUShort("ver", 0) == kVersion && prefs.getBytesLength("set") == sizeof(Settings)) {
    prefs.getBytes("set", &gSet, sizeof(Settings));
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
