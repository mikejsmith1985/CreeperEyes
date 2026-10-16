// settings.h -- user-adjustable settings, saved in flash (NVS) and edited from the
// built-in web page. Defaults here are what a freshly flashed board uses.
#pragma once
#include <Arduino.h>

struct EyeLook {          // one per eye: [0] = left (CS on GPIO17), [1] = right (CS on GPIO4)
  uint8_t  style;         // index into kEyeStyles (0 = human)
  uint8_t  irisMode;      // 0 = original colours, 1 = recolour iris with irisColor
  uint32_t irisColor;     // 0xRRGGBB
  uint32_t eyelidColor;   // 0xRRGGBB
};

struct Settings {
  // Look
  EyeLook  eye[2];
  bool     linkEyes;      // web page edits both eyes together
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

  // ---- added in v3 (kept at the end so v2 saves load as a prefix) ----
  uint32_t v3;            // 4-byte aligned start of the v3 block (unused)
  bool     kidMode;       // softer sounds, no scares, friendlier eyes, quieter
  bool     randomEyes;    // new random eye style each time he wakes up
  bool     transforms;    // eyes turn into dragon eyes when someone lingers
  bool     jumpScares;    // roar when someone rushes at him
  bool     antics;        // winks, eye rolls, cross-eyes, nodding off
  bool     sleepSounds;   // snores now and then while asleep
  bool     powerSaver;    // dim + slow down after a while asleep

  // ---- added in v4 ----
  uint16_t rangeMm;       // radar ignores anyone further away than this (back-of-car: 2 m)

  // ---- added in v5 ----
  uint8_t  followGain;    // 1..5: how exaggerated the eyes follow people (1 = realistic)
};

extern Settings gSet;
extern volatile bool gSettingsDirty;   // set by the web UI, consumed by the render loop

void     settingsLoad();
void     settingsSave();
void     settingsReset();
uint16_t rgb888to565(uint32_t c);
