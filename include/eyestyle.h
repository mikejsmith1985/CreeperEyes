// eyestyle.h -- one entry per selectable eye look. The graphics live in their own flash
// partition ('eyes', written from eyedata/eyes.bin built by tools/eyegen.py) so that the
// app stays small enough for wireless updates. eyesLoad() maps it into memory at boot.
#pragma once
#include <Arduino.h>

struct EyeStyle {
  const char     *name;
  const uint16_t *sclera;   uint16_t scleraW, scleraH;   // RGB565 image the eye moves over
  const uint16_t *iris;     uint16_t irisMapW, irisMapH; // iris texture (angle x distance)
  const uint8_t  *upper;                                 // 128x128 eyelid threshold maps
  const uint8_t  *lower;
  const uint16_t *polar;    uint16_t irisW, irisH;       // polar lookup for the iris area
  int16_t         irisMin,  irisMax;                     // pupil size range for this eye
};

#define MAX_EYE_STYLES 24
extern EyeStyle kEyeStyles[MAX_EYE_STYLES];
extern uint8_t  kEyeStyleCount;

bool eyesLoad();                         // false if the eyes partition is missing/blank
int  eyeStyleFind(const char *name);     // index by name, -1 if not found
