// puppet.h -- radar-driven behaviour and sound for the CreeperEyes bust.
//
// The eye renderer in main.cpp keeps doing all of the drawing. This module only
// decides WHERE the eyes look, how closed the lids are, how big the iris is and
// whether autonomous blinking is allowed, based on an HLK-LD2450 radar. It also
// plays sounds on a DFPlayer Mini.
//
// If no radar data arrives (radar unplugged or broken) every hook returns
// "no override" and the eyes behave exactly like the original UncannyEyes code.
#pragma once
#include <Arduino.h>

// ---------------- Wiring (ESP32 GPIO numbers) ----------------
#define RADAR_RX_PIN   32   // ESP32 RX  <- LD2450 TX
#define RADAR_TX_PIN   33   // ESP32 TX  -> LD2450 RX
#define RADAR_BAUD     256000
#define SOUND_RX_PIN   26   // ESP32 RX  <- DFPlayer TX
#define SOUND_TX_PIN   25   // ESP32 TX  -> 1k resistor -> DFPlayer RX
#define SOUND_BAUD     9600

// ---------------- Tuning ----------------
// Volume, wake/lean distances, sleep delay and gaze direction live in settings.h
// and are edited from the web page.
#define ZONE_HALF_WIDTH_MM  1500   // ignore people further to the side than this
#define RADAR_DEBUG_PRINT   1      // print radar targets to the serial monitor twice a second

// Sound files: microSD card, folder named "mp3", files named by number.
#define TRK_WAKE       1           // /mp3/0001.mp3  wake-up groan
#define TRK_GROWL_FIRST 2          // /mp3/0002.mp3 .. 0005.mp3 random growls
#define TRK_GROWL_LAST 5
#define TRK_LEAN       6           // /mp3/0006.mp3  "Mmm..." when someone leans in

void puppetSetup();                      // call once in setup()
void puppetUpdate(uint32_t nowMicros);   // call once per pair of eye frames

bool    puppetGaze(int16_t *x, int16_t *y); // true -> use x,y (0-1023) as eye position
uint8_t puppetLidClose();                   // 0 = normal, 255 = fully closed
int16_t puppetIris();                       // -1 = autonomous, else iris value (IRIS_MIN..IRIS_MAX)
bool    puppetAllowBlink();                 // false while asleep

// Used by the web page
void    puppetApplyVolume();                // push gSet.volume to the DFPlayer
void    puppetCommand(const String &cmd);   // "wake", "sleep", "sound:N"
String  puppetStatusJson();                 // radar / state snapshot for the web page
