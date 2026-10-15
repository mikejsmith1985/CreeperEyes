// eyestyle.h -- one entry per selectable eye look. The tables themselves are generated
// by tools/eyegen.py into src/eyedata/eyedata.inc and compiled by src/eyes.cpp.
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

extern const EyeStyle kEyeStyles[];
extern const uint8_t  kEyeStyleCount;
