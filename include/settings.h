// settings.h -- user-adjustable settings, saved in flash (NVS) and edited from the
// built-in web page. Defaults here are what a freshly flashed board uses.
#pragma once
#include <Arduino.h>

struct Settings {
  // Look
  uint8_t  eyeStyle;      // 0 = human (hazel), 1 = newt
  uint8_t  irisMode;      // 0 = original colours, 1 = recolour iris with irisColor
  uint32_t irisColor;     // 0xRRGGBB
  uint32_t eyelidColor;   // 0xRRGGBB
  uint8_t  brightness;    // 1..15 (OLED master contrast)
  bool     autoBlink;

  // Behaviour
  uint8_t  volume;        // 0..30
  uint16_t wakeDistMm;
  uint16_t leanDistMm;
  uint16_t sleepAfterS;
  bool     flipGazeX;
  bool     flipGazeY;

  // Wi-Fi
  char     wifiPass[33];  // AP password (8+ chars), network name is CreeperEyes-XXXX
};

extern Settings gSet;
extern volatile bool gSettingsDirty;   // set by the web UI, consumed by the render loop

void     settingsLoad();
void     settingsSave();
void     settingsReset();
uint16_t rgb888to565(uint32_t c);
