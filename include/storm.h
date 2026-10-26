// storm.h -- WS2812 LED strip behind the backdrop: a moody glow, candle-lit house windows and
// lightning strikes followed by thunder on the DFPlayer. Runs in its own task on core 0.
#pragma once
#include <Arduino.h>

#define STORM_PIN      27     // strip DIN (through a 330 ohm resistor)
#define STORM_ROWS     3      // strip laid in 3 rows on the back of the light box, zig-zag,
#define STORM_PER_ROW  18     //   starting bottom-left (wire exit), 18 LEDs per row at 60 LED/m (300 mm)
#define STORM_LEDS     (STORM_ROWS * STORM_PER_ROW)

void stormStart();            // call once in setup()
void stormStrike();           // lightning now (from the phone or a scare)
bool stormActive();           // a strike sequence is running
