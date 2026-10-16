// puppet.h -- radar-driven behaviour, phone puppeteering and sound for the CreeperEyes bust.
//
// The eye renderer in main.cpp keeps doing all of the drawing. This module only decides
// WHERE each eye looks, how closed each lid is, how big the pupils are, whether autonomous
// blinking is allowed, temporary eye-style changes, and screen brightness. It also plays
// sounds on a DFPlayer Mini and counts visitors.
//
// If no radar data arrives (radar unplugged or broken) the eyes behave like the original
// UncannyEyes code, except that phone puppet mode still works.
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
// Volume, distances, sleep delay, gaze direction and feature switches live in settings.h
// and are edited from the web page.
#define ZONE_HALF_WIDTH_MM  1200   // ignore people further to the side than this
// (how far out he looks is the "Detection range" setting, default 2 m)
#define RADAR_DEBUG_PRINT   0      // 1 = print radar targets to the serial monitor twice a second

// Sound files: microSD card, folder named "mp3", files named by number.
#define TRK_WAKE        1          // /mp3/0001.mp3  wake-up groan
#define TRK_GROWL_FIRST 2          // /mp3/0002.mp3 .. 0005.mp3 random growls
#define TRK_GROWL_LAST  5
#define TRK_LEAN        6          // /mp3/0006.mp3  "Mmm..." when someone leans in
#define TRK_ROAR        7          // /mp3/0007.mp3  jump-scare roar        (falls back to 0002)
#define TRK_SNORE       8          // /mp3/0008.mp3  snore while asleep     (skipped if missing)
#define TRK_HMM         9          // /mp3/0009.mp3  friendly "hmm?"        (falls back to 0006)
#define TRK_CHUCKLE     10         // /mp3/0010.mp3  chuckle                (falls back to 0006)

// Phone puppet actions (POST /api/puppet a=N)
enum PuppetAction : uint8_t {
  PA_NONE = 0, PA_WINK_L, PA_WINK_R, PA_BLINK, PA_CLOSE_TOGGLE, PA_CROSS, PA_ROLL, PA_LOOK,
  PA_RELEASE = 20,
};

void puppetEarly();                      // call first thing in setup()
void puppetSetup();                      // call once in setup()
void puppetUpdate(uint32_t nowMicros);   // call once per pair of eye frames

bool    puppetGaze(uint8_t eye, int16_t *x, int16_t *y); // true -> use x,y (0-1023) for this eye
uint8_t puppetLidClose(uint8_t eye);                     // 0 = normal, 255 = fully closed
int16_t puppetIris();                    // -1 = autonomous, else iris value (IRIS_MIN..IRIS_MAX)
bool    puppetAllowBlink();              // false while asleep, staring, puppeted closed...
int16_t puppetStyle(uint8_t eye);        // -1 = use settings, else temporary eye style index
uint16_t puppetStyleVersion();           // changes whenever puppetStyle() results change
uint8_t puppetBrightness();              // OLED contrast to use right now (power saver)
uint8_t puppetFrameDelayMs();            // extra delay per frame pair (power saver)

// Used by the web page (called from the web task; everything is handed to the render thread)
void    puppetApplyVolume();                 // push gSet.volume (kid mode aware) to the DFPlayer
void    puppetCommand(const String &cmd);    // "wake", "sleep", "sound:N"
void    puppetPreviewLook();                 // open his eyes for a few seconds (look changed while asleep)
void    puppetPhone(int x, int y, int action); // x,y 0-1023 or -1; action PuppetAction or 100+N sound
void    puppetResetVisitsTonight();
void    puppetSaveVisitsIfNeeded();          // call periodically from the web task
String  puppetStatusJson();                  // live snapshot for the web page
uint16_t puppetFilesOnCard();                // file count the DFPlayer reported at boot
