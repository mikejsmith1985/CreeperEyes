//--------------------------------------------------------------------------
// 2018 Modified by Laurent Moll for Uncanny Eyes costume
// https://www.hackster.io/projects/376a13/
// Based on Adafruit code  - Adafruit header below:
//
// Uncanny eyes for PJRC Teensy 3.1 with Adafruit 1.5" OLED (product #1431)
// or 1.44" TFT LCD (#2088).  This uses Teensy-3.1-specific features and
// WILL NOT work on normal Arduino or other boards!  Use 72 MHz (Optimized)
// board speed -- OLED does not work at 96 MHz.
//
// Adafruit invests time and resources providing this open source code,
// please support Adafruit and open-source hardware by purchasing products
// from Adafruit!
//
// Written by Phil Burgess / Paint Your Dragon for Adafruit Industries.
// MIT license.  SPI FIFO insight from Paul Stoffregen's ILI9341_t3 library.
// Inspired by David Boccabella's (Marcwolf) hybrid servo/OLED eye concept.
//--------------------------------------------------------------------------

#include <SPI.h>
#include <Adafruit_GFX.h>      // Core graphics lib for Adafruit displays
#include <HardwareSerial.h>    // Needed for 2nd serial port on ESP32

#include "eyestyle.h"
#include "storm.h"          // All selectable eye looks (see tools/eyegen.py)
#include "puppet.h"            // Radar-driven behaviour + DFPlayer sound
#include "settings.h"          // User settings (saved in flash)
#include "webui.h"             // Wi-Fi settings page (+ gOtaActive)

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 128

// Per-eye look, rebuilt from the settings by applySettings().
struct EyeRender {
  const EyeStyle *s;
  uint16_t eyelid;        // RGB565 eyelid colour
  bool     recolor;       // tint the iris with irisLut
  uint16_t irisLut[64];   // luminance (0-62) -> tinted RGB565
};
static EyeRender er[2];

// DISPLAY HARDWARE CONFIG -------------------------------------------------

#include <Adafruit_SSD1351.h>  // OLED display library -OR-

typedef Adafruit_SSD1351 displayType; // Using OLED display(s)

#define DISPLAY_DC      16 // Data/command pin for BOTH displays
#define DISPLAY_RESET   5 // Reset pin for BOTH displays
#define SELECT_L_PIN    17 // LEFT eye chip select pin
#define SELECT_R_PIN    04 // RIGHT eye chip select pin

// INPUT CONFIG (for eye motion -- enable or comment out as needed) --------

//#define TRACKING        // If enabled, eyelid tracks pupil (off: lids stay open except when blinking)
#define IRIS_SMOOTH       // If enabled, filter input from IRIS_PIN
#define TRACK_TRAVEL_X 56  // px the iris may move left/right of centre while following someone
#define TRACK_TRAVEL_Y 40  // px up/down
#define IRIS_MIN      150 // Shared pupil animation range; each eye style maps it onto its own range
#define IRIS_MAX      400
#define AUTOBLINK        // If enabled, eyes blink autonomously
// Eyelid colour, iris colour, eye style, brightness and blinking are set from the
// web page (see settings.h) and applied by applySettings() below.

// Probably don't need to edit any config below this line, -----------------
// unless building a single-eye project (pendant, etc.), in which case one
// of the two elements in the eye[] array further down can be commented out.

// Eye blinks are a tiny 3-state machine.  Per-eye allows winks + blinks.
#define NOBLINK 0     // Not currently engaged in a blink
#define ENBLINK 1     // Eyelid is currently closing
#define DEBLINK 2     // Eyelid is currently opening
typedef struct {
  uint8_t  state;     // NOBLINK/ENBLINK/DEBLINK
  uint32_t duration;  // Duration of blink state (micros)
  uint32_t startTime; // Time (micros) of last state change
} eyeBlink;

#define MOSI_PIN  23
#define SCLK_PIN  18

SPISettings settings(16000000, MSBFIRST, SPI_MODE3); // 26.667MHz seems reliable on the ESP32.
struct {
  displayType display; // OLED/TFT object
  uint8_t     cs;      // Chip select pin
  eyeBlink    blink;   // Current blink state
} eye[] = { // OK to comment out one of these for single-eye display:
  // displayType(SELECT_L_PIN,DISPLAY_DC,0),SELECT_L_PIN,{NOBLINK},
  Adafruit_SSD1351(128, 128, &SPI, SELECT_L_PIN, DISPLAY_DC, -1), SELECT_L_PIN,{NOBLINK},
  Adafruit_SSD1351(128, 128, &SPI, SELECT_R_PIN, DISPLAY_DC, -1), SELECT_R_PIN,{NOBLINK},
  // displayType(SELECT_R_PIN,DISPLAY_DC,0),SELECT_R_PIN,{NOBLINK},
};
#define NUM_EYES (sizeof(eye) / sizeof(eye[0]))

// INITIALIZATION -- runs once at startup ----------------------------------

void applySettings();

void setup(void) {
  uint8_t e;

  Serial.begin(921600);
  puppetEarly();
  settingsLoad();
  randomSeed(analogRead(A3)); // Seed random() from floating analog input

  // Both displays share a common reset line; 0 is passed to display
  // constructor (so no reset in begin()) -- must reset manually here:
  pinMode(DISPLAY_RESET, OUTPUT);
  digitalWrite(DISPLAY_RESET, LOW);  delay(1);
  digitalWrite(DISPLAY_RESET, HIGH); delay(50);

  for(e=0; e<NUM_EYES; e++) { // Deselect all
    pinMode(eye[e].cs, OUTPUT);
    digitalWrite(eye[e].cs, HIGH);
  }
  for(e=0; e<NUM_EYES; e++) {
    digitalWrite(eye[e].cs, LOW); // Select one eye for init
    eye[e].display.begin();
    digitalWrite(eye[e].cs, HIGH); // Deselect
  }

  eyesLoad();                      // map eye graphics from the 'eyes' flash partition
  applySettings(); gSettingsDirty = false;   // eye looks, colours, brightness
  puppetSetup();                   // radar + sound (falls back to normal eyes if radar absent)
  webuiStart();                    // CreeperEyes-XXXX Wi-Fi + settings page
  stormStart();                    // backdrop LEDs: glow, candle windows, lightning
  
  // One of the displays is configured to mirror on the X axis.  Simplifies
  // eyelid handling in the drawEye() function -- no need for distinct
  // L-to-R or R-to-L inner loops.  Just the X coordinate of the iris is
  // then reversed when drawing this eye, so they move the same.  Magic!
  // eye[0].display.writeCommand(SSD1351_CMD_SETREMAP);
  // eye[0].display.write16(0x76);
}


// EYE-RENDERING FUNCTION --------------------------------------------------9
void drawEye( // Renders one eye.  Inputs must be pre-clipped & valid.
  uint8_t  e,       // Eye array index; 0 or 1 for left/right
  uint16_t iScale,  // Iris scale 0-1023 (bigger = bigger pupil), already in this style's range
  int16_t  scleraX, // First pixel X offset into sclera image (may run past the edges:
  int16_t  scleraY, // the sclera's edge pixels are repeated, so the iris can travel further)
  uint8_t  uT,      // Upper eyelid threshold value
  uint8_t  lT) {    // Lower eyelid threshold value

  const EyeRender &r = er[e];
  static uint16_t pBurst[SCREEN_WIDTH*SCREEN_HEIGHT]; // Full frame buffer possible on ESP32
  if(!r.s) {                                          // eye graphics missing: dim red screen
    for(int i = 0; i < SCREEN_WIDTH*SCREEN_HEIGHT; i++) pBurst[i] = 0x2000;
    eye[e].display.startWrite();
    eye[e].display.writeCommand(SSD1351_CMD_SETROW);    eye[e].display.write16(0); eye[e].display.write16(SCREEN_HEIGHT - 1);
    eye[e].display.writeCommand(SSD1351_CMD_SETCOLUMN); eye[e].display.write16(0); eye[e].display.write16(SCREEN_WIDTH - 1);
    eye[e].display.writeCommand(SSD1351_CMD_WRITERAM);
    SPI.writePixels((uint8_t*)pBurst, sizeof(pBurst));
    eye[e].display.endWrite();
    return;
  }
  const EyeStyle  &s = *r.s;
  uint8_t  screenX, screenY;
  int16_t  scleraXsave;
  int16_t  irisX, irisY;
  uint16_t p, a, burstIdx;
  uint32_t d;

  uint8_t  irisThreshold = (128 * (1023 - iScale) + 512) / 1024;
  if(irisThreshold < 1) irisThreshold = 1;
  uint32_t irisScale = (uint32_t)s.irisMapH * 65536 / irisThreshold;

  scleraXsave = scleraX; // Save initial X value to reset on each line
  irisY       = scleraY - (s.scleraH - s.irisH) / 2;
  burstIdx = 0;
  for(screenY=0; screenY<SCREEN_HEIGHT; screenY++, scleraY++, irisY++) {
    scleraX = scleraXsave;
    irisX   = scleraXsave - (s.scleraW - s.irisW) / 2;
    const uint8_t  *upRow = s.upper  + screenY * SCREEN_WIDTH;
    const uint8_t  *loRow = s.lower  + screenY * SCREEN_WIDTH;
    const uint16_t *scRow = s.sclera + constrain(scleraY, 0, (int)s.scleraH - 1) * s.scleraW;
    const int16_t   scMaxX = s.scleraW - 1;
    for(screenX=0; screenX<SCREEN_WIDTH; screenX++, scleraX++, irisX++) {
      int effectiveX = e == 1 ? screenX : SCREEN_WIDTH - 1 - screenX;
      if((loRow[effectiveX] <= lT) || (upRow[effectiveX] <= uT)) {   // Covered by eyelid
        p = r.eyelid;
      } else if((irisY < 0) || (irisY >= s.irisH) ||
                (irisX < 0) || (irisX >= s.irisW)) {              // In sclera
        p = scRow[scleraX < 0 ? 0 : scleraX > scMaxX ? scMaxX : scleraX];
      } else {                                                    // Maybe iris...
        p = s.polar[irisY * s.irisW + irisX];                     // Polar angle/dist
        d = p & 0x7F;                                             // Distance (0-127)
        if(d < irisThreshold) {                                   // Within scaled iris area
          d = d * irisScale >> 16;                                // -> iris image row
          if(d >= s.irisMapH) d = s.irisMapH - 1;
          a = (s.irisMapW * (p >> 7)) / 512;                      // Angle (X)
          p = s.iris[d * s.irisMapW + a];
          if(r.recolor) p = r.irisLut[(((p >> 11) * 154) + (((p >> 5) & 63) * 150) + ((p & 31) * 58)) >> 8];
        } else {
          p = scRow[scleraX < 0 ? 0 : scleraX > scMaxX ? scMaxX : scleraX];                                     // Pupil / sclera
        }
      }
      pBurst[burstIdx++] = p;
    }
  }
  
  eye[e].display.startWrite();
  eye[e].display.writeCommand(SSD1351_CMD_SETROW);    // Y range
  eye[e].display.write16(0x0);
  eye[e].display.write16(SCREEN_HEIGHT - 1);
  eye[e].display.writeCommand(SSD1351_CMD_SETCOLUMN); // X range
  eye[e].display.write16(0x0);
  eye[e].display.write16(SCREEN_WIDTH  - 1);
  eye[e].display.writeCommand(SSD1351_CMD_WRITERAM);  // Begin write

  // For ESP32, use writePixels function to transfer the whole framebuffer in one large burst
  SPI.writePixels((uint8_t*)pBurst, sizeof(pBurst));
  eye[e].display.endWrite();
}


// EYE ANIMATION -----------------------------------------------------------

const uint8_t ease[] = { // Ease in/out curve for eye movements 3*t^2-2*t^3
    0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  2,  2,  2,  3,   // T
    3,  3,  4,  4,  4,  5,  5,  6,  6,  7,  7,  8,  9,  9, 10, 10,   // h
   11, 12, 12, 13, 14, 15, 15, 16, 17, 18, 18, 19, 20, 21, 22, 23,   // x
   24, 25, 26, 27, 27, 28, 29, 30, 31, 33, 34, 35, 36, 37, 38, 39,   // 2
   40, 41, 42, 44, 45, 46, 47, 48, 50, 51, 52, 53, 54, 56, 57, 58,   // A
   60, 61, 62, 63, 65, 66, 67, 69, 70, 72, 73, 74, 76, 77, 78, 80,   // l
   81, 83, 84, 85, 87, 88, 90, 91, 93, 94, 96, 97, 98,100,101,103,   // e
  104,106,107,109,110,112,113,115,116,118,119,121,122,124,125,127,   // c
  128,130,131,133,134,136,137,139,140,142,143,145,146,148,149,151,   // J
  152,154,155,157,158,159,161,162,164,165,167,168,170,171,172,174,   // a
  175,177,178,179,181,182,183,185,186,188,189,190,192,193,194,195,   // c
  197,198,199,201,202,203,204,205,207,208,209,210,211,213,214,215,   // o
  216,217,218,219,220,221,222,224,225,226,227,228,228,229,230,231,   // b
  232,233,234,235,236,237,237,238,239,240,240,241,242,243,243,244,   // s
  245,245,246,246,247,248,248,249,249,250,250,251,251,251,252,252,   // o
  252,253,253,253,254,254,254,254,254,255,255,255,255,255,255,255 }; // n

#ifdef AUTOBLINK
uint32_t timeOfLastBlink = 0L, timeToNextBlink = 0L;
#endif

#define BCAST_ADDR 0
#define SER_CMD_SIZE 9
uint16_t serAddr = 1;

// Applies the web-page settings. Runs on the render thread between frames.
void applySettings() {
  for(uint8_t e = 0; e < NUM_EYES; e++) {
    const EyeLook &L = gSet.eye[e];
    EyeRender &r = er[e];
    int16_t st = puppetStyle(e) >= 0 ? puppetStyle(e) : L.style;   // temporary looks win
    r.s       = kEyeStyleCount ? &kEyeStyles[st < kEyeStyleCount ? st : 0] : nullptr;
    r.eyelid  = rgb888to565(L.eyelidColor);
    r.recolor = L.irisMode == 1;
    if(r.recolor) {                   // keep the iris texture, change its hue
      uint32_t c = L.irisColor;
      uint16_t cr = (c >> 16) & 0xFF, cg = (c >> 8) & 0xFF, cb = c & 0xFF;
      for(int i = 0; i < 64; i++) {
        uint16_t lum = min(255, i * 255 / 62 * 2);
        r.irisLut[i] = rgb888to565(((cr * lum / 255) << 16) | ((cg * lum / 255) << 8) | (cb * lum / 255));
      }
    }
  }
  // brightness is applied in frame() (it also handles the power saver)
  static const EyeStyle *was[2] = {nullptr, nullptr};
  if(er[0].s != was[0] || er[1].s != was[1]) {
    was[0] = er[0].s; was[1] = er[1].s;
    Serial.printf("[look] left=%s right=%s\n", er[0].s ? er[0].s->name : "-", er[1].s ? er[1].s->name : "-");
  }
}

void frame( // Process motion for a single frame of left or right eye
  uint16_t        iScale) {     // Iris scale (0-1023) passed in
  static uint32_t frames   = 0; // Used in frame rate calculation
  static uint8_t  eyeIndex = 0; // eye[] array counter
  int16_t         eyeX, eyeY;
  uint32_t        t; // Time at start of function
  static char     serCmd[SER_CMD_SIZE+1];
  static uint16_t serCmdIdx = 0;
  static uint16_t serNewEyeCtrl = 0;
  static uint16_t serEyeCtrl = 0;

  if(gOtaActive) { delay(20); return; }      // wireless update in progress: stop drawing
  if(++eyeIndex >= NUM_EYES) eyeIndex = 0; // Cycle through eyes, 1 per call
  if(eyeIndex == 0) {
    static uint16_t styleVer = 0;
    if(gSettingsDirty || styleVer != puppetStyleVersion()) {
      gSettingsDirty = false; styleVer = puppetStyleVersion(); applySettings();
    }
    static uint8_t contrastNow = 0;            // power saver dims the screens while asleep
    uint8_t c = constrain(puppetBrightness(), 1, 15);
    if(c != contrastNow) {
      contrastNow = c;
      for(uint8_t e = 0; e < NUM_EYES; e++) eye[e].display.sendCommand(SSD1351_CMD_CONTRASTMASTER, &c, 1);
    }
    if(puppetFrameDelayMs()) delay(puppetFrameDelayMs());
    puppetUpdate(micros());                   // radar/sound behaviour, once per eye pair
  }
  { int16_t io = puppetIris(); if(io >= 0) iScale = io; }

  // X/Y movement

  t = micros();

  // Autonomous X/Y eye motion
  // Periodically initiates motion to a new random point, random speed,
  // holds there for random period until next motion.

  static boolean  eyeInMotion      = false;
  static int16_t  eyeOldX=512, eyeOldY=512, eyeCurX=512, eyeCurY=512, eyeNewX=512, eyeNewY=512;
  static uint32_t eyeMoveStartTime = 0L;
  static int32_t  eyeMoveDuration  = 0L;
  
  int32_t dt = t - eyeMoveStartTime;      // uS elapsed since last eye event

  if(eyeInMotion) {                       // Currently moving?
    if(dt >= eyeMoveDuration) {           // Time up?  Destination reached.
      if (serEyeCtrl) { // If serial controlled, we're done moving, but stay in motion
        eyeX = eyeOldX = eyeNewX;           // Save position
        eyeY = eyeOldY = eyeNewY;
      } else {
        eyeInMotion      = false;           // Stop moving
        eyeMoveDuration  = random(3000000); // 0-3 sec stop
        eyeMoveStartTime = t;               // Save initial time of stop
        eyeX = eyeOldX = eyeNewX;           // Save position
        eyeY = eyeOldY = eyeNewY;
      }
    } else { // Move time's not yet fully elapsed -- interpolate position
      int16_t e = ease[255 * dt / eyeMoveDuration] + 1;   // Ease curve
      eyeX = eyeOldX + (((eyeNewX - eyeOldX) * e) / 256); // Interp X
      eyeY = eyeOldY + (((eyeNewY - eyeOldY) * e) / 256); // and Y
    }
  } else {                                // Eye stopped
    eyeX = eyeOldX;
    eyeY = eyeOldY;
    if(dt > eyeMoveDuration) {            // Time up?  Begin new move.
      int16_t  dx, dy;
      uint32_t d;
      do {                                // Pick new dest in circle
        eyeNewX = random(1024);
        eyeNewY = random(1024);
        dx      = (eyeNewX * 2) - 1023;
        dy      = (eyeNewY * 2) - 1023;
      } while((d = (dx * dx + dy * dy)) > (1023 * 1023)); // Keep trying
      eyeMoveDuration  = random(72000, 144000); // ~1/14 - ~1/7 sec
      eyeMoveStartTime = t;               // Save initial time of move
      eyeInMotion      = true;            // Start move on next frame
    }
  }
  bool tracked = false;
  { // Radar gaze override: when someone is tracked the eyes follow them.
    int16_t gx, gy;
    if(puppetGaze(eyeIndex, &gx, &gy)) {
      tracked = true;
      eyeX = eyeOldX = eyeNewX = gx;            // keep autonomous state in sync so it
      eyeY = eyeOldY = eyeNewY = gy;            // resumes smoothly when override ends
      eyeInMotion = false; eyeMoveStartTime = t; eyeMoveDuration = 1500000;
    }
  }
  eyeCurX = eyeX;
  eyeCurY = eyeY;
  
  // Blinking
#ifdef AUTOBLINK
  // Similar to the autonomous eye movement above -- blink start times
  // and durations are random (within ranges).
  if(gSet.autoBlink && puppetAllowBlink() && (t - timeOfLastBlink) >= timeToNextBlink) { // Start new blink?
    timeOfLastBlink = t;
    uint32_t blinkDuration = random(36000, 72000); // ~1/28 - ~1/14 sec
    // Set up durations for both eyes (if not already winking)
    for(uint8_t e=0; e<NUM_EYES; e++) {
      if(eye[e].blink.state == NOBLINK) {
        eye[e].blink.state     = ENBLINK;
        eye[e].blink.startTime = t;
        eye[e].blink.duration  = blinkDuration;
      }
    }
    timeToNextBlink = blinkDuration * 3 + random(4000000);
  }
#endif

  if(eye[eyeIndex].blink.state) { // Eye currently blinking?
    // Check if current blink state time has elapsed
    if((t - eye[eyeIndex].blink.startTime) >= eye[eyeIndex].blink.duration) {
      // No buttons, or other state...
        if(++eye[eyeIndex].blink.state > DEBLINK) { // Deblinking finished?
          eye[eyeIndex].blink.state = NOBLINK;      // No longer blinking
        } else { // Advancing from ENBLINK to DEBLINK mode
          eye[eyeIndex].blink.duration *= 2; // DEBLINK is 1/2 ENBLINK speed
          eye[eyeIndex].blink.startTime = t;
        }
     
    }
  }

  // Process motion, blinking and iris scale into renderable values

  // Iris scaling: the shared IRIS_MIN..IRIS_MAX animation is mapped onto this eye style's range
  if(!er[eyeIndex].s) { drawEye(eyeIndex, 0, 0, 0, 0, 0); return; }
  const EyeStyle &st = *er[eyeIndex].s;
  int32_t iv = st.irisMin + ((int32_t)iScale - IRIS_MIN) * (st.irisMax - st.irisMin) / (IRIS_MAX - IRIS_MIN);
  iScale = constrain(iv, 0, 1023);

  // Scale eye X/Y positions (0-1023) to pixel units used by drawEye()
  if(tracked) {
    // Following someone: exaggerated travel, the iris can go almost to the screen edge
    // (further than the sclera image allows -- drawEye() repeats the sclera's edge pixels).
    eyeX = (st.scleraW - 128) / 2 + ((int32_t)eyeX - 512) * TRACK_TRAVEL_X / 512;
    eyeY = (st.scleraH - 128) / 2 + ((int32_t)eyeY - 512) * TRACK_TRAVEL_Y / 512;
  } else {
    eyeX = map(eyeX, 0, 1023, 0, st.scleraW - 128);
    eyeY = map(eyeY, 0, 1023, 0, st.scleraH - 128);
  }
  if (eyeIndex == 1) { // this inverts the motion of the eyes
    // eyeX = (SCLERA_WIDTH - 128) - eyeX; // Mirrored display
  }

  // Horizontal position is offset so that eyes are very slightly crossed
  // to appear fixated (converged) at a conversational distance.  Number
  // here was extracted from my posterior and not mathematically based.
  // I suppose one could get all clever with a range sensor, but for now...
  eyeX += 4;
  if(!tracked && eyeX > (st.scleraW - 128)) eyeX = (st.scleraW - 128);

  // Eyelids are rendered using a brightness threshold image.  This same
  // map can be used to simplify another problem: making the upper eyelid
  // track the pupil (eyes tend to open only as much as needed -- e.g. look
  // down and the upper eyelid drops).  Just sample a point in the upper
  // lid map slightly above the pupil to determine the rendering threshold.
  static uint8_t uThreshold = 128;
  uint8_t        lThreshold, n;
#ifdef TRACKING
#error TRACKING is not supported with per-eye styles
#else // No tracking -- eyelids full open unless blink modifies them
  uThreshold = lThreshold = 0;
#endif

  // The upper/lower thresholds are then scaled relative to the current
  // blink position so that blinks work together with pupil tracking.
  if(eye[eyeIndex].blink.state) { // Eye currently blinking?
    uint32_t s = (t - eye[eyeIndex].blink.startTime);
    if(s >= eye[eyeIndex].blink.duration) s = 255;   // At or past blink end
    else s = 255 * s / eye[eyeIndex].blink.duration; // Mid-blink
    s          = (eye[eyeIndex].blink.state == DEBLINK) ? 1 + s : 256 - s;
    n          = (uThreshold * s + 254 * (257 - s)) / 256;
    lThreshold = (lThreshold * s + 254 * (257 - s)) / 256;
  } else {
    n          = uThreshold;
  }

  // Extra lid closure from the behaviour module (sleeping, waking).
  uint8_t lc = puppetLidClose(eyeIndex);
  if(lc) {
    n          = n          + ((254 - n)          * lc) / 255;
    lThreshold = lThreshold + ((254 - lThreshold) * lc) / 255;
  }

  // Pass all the derived values to the eye-rendering function:
  drawEye(eyeIndex, iScale, eyeX, eyeY, n, lThreshold);
}


// AUTONOMOUS IRIS SCALING (if no photocell or dial) -----------------------
// Autonomous iris motion uses a fractal behavior to similate both the major
// reaction of the eye plus the continuous smaller adjustments that occur.

uint16_t oldIris = (IRIS_MIN + IRIS_MAX) / 2, newIris;

void split( // Subdivides motion path into two sub-paths w/randomization
  int16_t  startValue, // Iris scale value (IRIS_MIN to IRIS_MAX) at start
  int16_t  endValue,   // Iris scale value at end
  uint32_t startTime,  // micros() at start
  int32_t  duration,   // Start-to-end time, in microseconds
  int16_t  range) {    // Allowable scale value variance when subdividing

  if(range >= 8) {     // Limit subdvision count, because recursion
    range    /= 2;     // Split range & time in half for subdivision,
    duration /= 2;     // then pick random center point within range:
    int16_t  midValue = (startValue + endValue - range) / 2 + random(range);
    uint32_t midTime  = startTime + duration;
    split(startValue, midValue, startTime, duration, range); // First half
    split(midValue  , endValue, midTime  , duration, range); // Second half
  } else {             // No more subdivisons, do iris motion...
    int32_t dt;        // Time (micros) since start of motion
    int16_t v;         // Interim value
    while((dt = (micros() - startTime)) < duration) {
      v = startValue + (((endValue - startValue) * dt) / duration);
      if(v < IRIS_MIN)      v = IRIS_MIN; // Clip just in case
      else if(v > IRIS_MAX) v = IRIS_MAX;
      frame(v);        // Draw frame w/interim iris scale value
    }
  }
}

// MAIN LOOP -- runs continuously after setup() ----------------------------

void loop() {

// Autonomous iris scaling -- invoke recursive function

  newIris = random(IRIS_MIN, IRIS_MAX);
  
  split(oldIris, newIris, micros(), 10000000L, IRIS_MAX - IRIS_MIN);
  oldIris = newIris;
}