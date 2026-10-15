// puppet.cpp -- see include/puppet.h for what this does and how it is wired.
#include "puppet.h"
#include "settings.h"
#include <HardwareSerial.h>
#include <math.h>

// The iris range used by main.cpp (kept in sync with IRIS_MIN / IRIS_MAX there).
static const int16_t kIrisMin = 150, kIrisMax = 400;

static HardwareSerial radarSerial(2);
static HardwareSerial soundSerial(1);

// =====================================================================
// LD2450 radar: 30-byte frames  AA FF 03 00 | 3 x 8-byte targets | 55 CC
// Each target: x, y, speed (int16 where bit15 set = positive), resolution.
// Units are millimetres; y is distance straight out from the radar.
// =====================================================================
struct Target { bool valid; int16_t x, y; };
static Target   targets[3];
static uint32_t lastRadarFrameMs = 0;
static bool     radarSeen = false;

static int16_t decodeSigned(uint16_t raw) {
  int16_t v = raw & 0x7FFF;
  return (raw & 0x8000) ? v : -v;
}

static void parseFrame(const uint8_t *f) {
  for (int i = 0; i < 3; i++) {
    const uint8_t *p = f + 4 + i * 8;
    uint16_t rx = p[0] | (p[1] << 8), ry = p[2] | (p[3] << 8);
    targets[i].valid = (rx | ry) != 0;
    targets[i].x = decodeSigned(rx);
    targets[i].y = decodeSigned(ry);
  }
  lastRadarFrameMs = millis();
  radarSeen = true;
}

static void pollRadar() {
  static uint8_t buf[30];
  static uint8_t len = 0;
  static const uint8_t head[4] = {0xAA, 0xFF, 0x03, 0x00};
  while (radarSerial.available()) {
    uint8_t b = radarSerial.read();
    if (len < 4) {                       // hunt for the 4-byte header
      if (b == head[len]) buf[len++] = b;
      else len = (b == head[0]) ? (buf[0] = b, 1) : 0;
      continue;
    }
    buf[len++] = b;
    if (len == 30) {
      if (buf[28] == 0x55 && buf[29] == 0xCC) parseFrame(buf);
      len = 0;
    }
  }
}

static bool radarAlive() { return radarSeen && (millis() - lastRadarFrameMs) < 3000; }

// Nearest person inside the zone in front of the bust. Returns false if none.
static bool nearestTarget(Target &out, float &distMm) {
  bool found = false; distMm = 1e9;
  for (auto &t : targets) {
    if (!t.valid || t.y <= 0 || abs(t.x) > ZONE_HALF_WIDTH_MM) continue;
    float d = sqrtf((float)t.x * t.x + (float)t.y * t.y);
    if (d < distMm) { distMm = d; out = t; found = true; }
  }
  return found;
}

// =====================================================================
// DFPlayer Mini: 10-byte commands, no library needed.
// Command 0x12 plays /mp3/NNNN.mp3 by its number (not by copy order).
// =====================================================================
static uint32_t soundBusyUntilMs = 0;

static void dfSend(uint8_t cmd, uint16_t param) {
  uint8_t m[10] = {0x7E, 0xFF, 0x06, cmd, 0x00, (uint8_t)(param >> 8), (uint8_t)param, 0, 0, 0xEF};
  uint16_t sum = 0;
  for (int i = 1; i < 7; i++) sum += m[i];
  sum = -sum;
  m[7] = sum >> 8; m[8] = sum & 0xFF;
  soundSerial.write(m, 10);
}

// Read and log whatever the DFPlayer says back, so wiring/card problems show in the serial monitor.
static bool dfHeard = false;
static void pollDf() {
  static uint8_t f[10]; static uint8_t n = 0;
  while (soundSerial.available()) {
    uint8_t b = soundSerial.read();
    if (n == 0 && b != 0x7E) continue;
    f[n++] = b;
    if (n < 10) continue;
    n = 0;
    if (f[9] != 0xEF) continue;
    dfHeard = true;
    uint8_t cmd = f[3]; uint16_t p = (f[5] << 8) | f[6];
    const char *what = "";
    switch (cmd) {
      case 0x3F: what = (p & 0x02) ? "ready, SD card found" : "ready, NO SD card"; break;
      case 0x3A: what = "card inserted"; break;
      case 0x3B: what = "card removed"; break;
      case 0x3D: what = "finished track"; break;
      case 0x40: what = p == 6 ? "ERROR: file not found" : p == 1 ? "ERROR: busy / card not ready" : "ERROR"; break;
      case 0x41: what = "ack"; break;
      case 0x48: what = "files on card"; break;
    }
    Serial.printf("[sound] DFPlayer says %02X %u %s\n", cmd, p, what);
  }
}

// Plays a track unless another sound started less than minGapMs ago (or force).
static void playTrack(uint16_t n, uint32_t lengthMs, bool force = false) {
  uint32_t now = millis();
  if (!force && now < soundBusyUntilMs) return;
  dfSend(0x12, n);
  soundBusyUntilMs = now + lengthMs;
  Serial.printf("[sound] track %u\n", n);
}

// =====================================================================
// Behaviour state machine
// =====================================================================
enum State { NO_RADAR, ASLEEP, WAKING, AWAKE };
static State    state = NO_RADAR;
static uint32_t stateSinceMs = 0, lastSeenMs = 0, nextGrowlMs = 0, leanCooldownMs = 0, nextTwitchMs = 0;
static float    lid = 0;            // current lid closure 0..255
static float    lidTarget = 0;
static float    gazeX = 512, gazeY = 512, gazeTX = 512, gazeTY = 512;
static int16_t  irisOverride = -1;
static uint32_t irisUntilMs = 0;
static uint32_t lastUpdateMs = 0;
static volatile int pendingCmd = 0;     // 1 wake, 2 sleep, 100+N play sound N, 200 volume
static Target   lastTarget = {false, 0, 0};
static float    lastDist = 0;
static bool     lastSomeone = false;

static void setState(State s) {
  static const char *names[] = {"NO_RADAR", "ASLEEP", "WAKING", "AWAKE"};
  state = s; stateSinceMs = millis();
  Serial.printf("[puppet] -> %s\n", names[s]);
}

void puppetSetup() {
  radarSerial.begin(RADAR_BAUD, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
  soundSerial.begin(SOUND_BAUD, SERIAL_8N1, SOUND_RX_PIN, SOUND_TX_PIN);
  // DFPlayer needs 1-3 s after power-up (clones are slower). Wait for its "ready" message.
  for (uint32_t t0 = millis(); millis() - t0 < 3000 && !dfHeard; ) { pollDf(); delay(20); }
  if (!dfHeard) Serial.println("[sound] no reply from DFPlayer yet (check its power and the P26 <- TX wire)");
  dfSend(0x06, gSet.volume);         // set volume
  delay(100);
  dfSend(0x48, 0);                   // ask how many files are on the card
  for (uint32_t t0 = millis(); millis() - t0 < 300; ) { pollDf(); delay(10); }
  playTrack(TRK_WAKE, 3000, true);   // boot test: you should hear the groan once
  Serial.println("[puppet] started. Radar on GPIO32/33 @256000, DFPlayer on GPIO26/25 @9600");
}

static float approach(float cur, float target, float rate, float dtS) {
  float k = 1.0f - expf(-rate * dtS);
  return cur + (target - cur) * k;
}

void puppetUpdate(uint32_t nowMicros) {
  (void)nowMicros;
  uint32_t now = millis();
  float dt = lastUpdateMs ? (now - lastUpdateMs) / 1000.0f : 0.0f;
  lastUpdateMs = now;

  pollRadar();
  pollDf();

  int cmd = pendingCmd; pendingCmd = 0;
  if (cmd == 200) dfSend(0x06, gSet.volume);
  else if (cmd >= 100) playTrack(cmd - 100, 1000, true);

  if (!radarAlive()) {               // no radar -> original behaviour
    if (state != NO_RADAR) setState(NO_RADAR);
    lid = lidTarget = 0; irisOverride = -1;
    return;
  }
  if (state == NO_RADAR) { setState(ASLEEP); nextTwitchMs = now + 5000; }

  Target t; float dist;
  bool someone = nearestTarget(t, dist);
  if (someone) lastSeenMs = now;
  lastSomeone = someone; if (someone) { lastTarget = t; lastDist = dist; }
  if (cmd == 1 && state == ASLEEP) { someone = true; dist = 1000; t = {true, 0, 1000}; lastSeenMs = now; }
  if (cmd == 2 && state != ASLEEP) setState(ASLEEP);

#if RADAR_DEBUG_PRINT
  static uint32_t lastPrint = 0;
  if (now - lastPrint > 500) {
    lastPrint = now;
    if (someone) Serial.printf("[radar] nearest x=%d mm  y=%d mm  dist=%.0f mm\n", t.x, t.y, dist);
    else         Serial.println("[radar] nobody in zone");
  }
#endif

  switch (state) {
    case ASLEEP:
      lidTarget = 238;               // a sleepy sliver
      gazeTX = 512; gazeTY = 512 - 120 * (gSet.flipGazeY ? -1 : 1);     // eyes rolled down a bit
      if (now > nextTwitchMs) {      // occasional twitch while sleeping
        lid = 190;
        nextTwitchMs = now + random(8000, 20000);
      }
      if (someone && dist < gSet.wakeDistMm) {
        setState(WAKING);
        playTrack(TRK_WAKE, 3000, true);
        irisOverride = kIrisMax; irisUntilMs = now + 1500;   // pupils snap wide
      }
      break;

    case WAKING:
      lidTarget = 0;
      if (now - stateSinceMs > 700) { setState(AWAKE); nextGrowlMs = now + random(15000, 30000); }
      // fall through so gaze follows during wake-up
    case AWAKE:
      if (someone) {
        float ang = atan2f((float)t.x, (float)t.y);                 // radians, 0 = straight ahead
        float gx = 512 + (gSet.flipGazeX ? -1 : 1) * (ang / 1.05f) * 430;          // +-60 deg -> +-430
        float down = constrain((2000.0f - dist) / 1600.0f, 0.0f, 1.0f); // closer kids -> look down
        float gy = 512 - (gSet.flipGazeY ? -1 : 1) * down * 300;
        gazeTX = constrain(gx, 80.0f, 943.0f);
        gazeTY = constrain(gy, 80.0f, 943.0f);

        if (dist < gSet.leanDistMm && now > leanCooldownMs) {      // leaning in!
          playTrack(TRK_LEAN, 2500, true);
          irisOverride = kIrisMin; irisUntilMs = now + 2000;        // pupils shrink
          lid = 0;
          leanCooldownMs = now + 6000;
        }
        if (now > nextGrowlMs) {
          playTrack(random(TRK_GROWL_FIRST, TRK_GROWL_LAST + 1), 3500);
          nextGrowlMs = now + random(20000, 40000);
        }
      }
      if (state == AWAKE && now - lastSeenMs > (gSet.sleepAfterS * 1000UL)) setState(ASLEEP);
      break;

    default: break;
  }

  if (irisOverride >= 0 && now > irisUntilMs) irisOverride = -1;

  // Smooth everything so motion looks organic.
  float lidRate = (state == ASLEEP) ? 1.2f : 10.0f;   // fall asleep slowly, wake fast
  lid   = approach(lid, lidTarget, lidRate, dt);
  gazeX = approach(gazeX, gazeTX, 6.0f, dt);
  gazeY = approach(gazeY, gazeTY, 6.0f, dt);
}

bool puppetGaze(int16_t *x, int16_t *y) {
  if (state == NO_RADAR) return false;
  // Keep inside the circle the original code uses for eye positions.
  float dx = gazeX - 512, dy = gazeY - 512, r = sqrtf(dx * dx + dy * dy);
  if (r > 480) { dx *= 480 / r; dy *= 480 / r; }
  *x = (int16_t)(512 + dx); *y = (int16_t)(512 + dy);
  return true;
}

uint8_t puppetLidClose()   { return (uint8_t)constrain(lid, 0.0f, 255.0f); }
int16_t puppetIris()       { return irisOverride; }
bool    puppetAllowBlink() { return state != ASLEEP; }

void puppetApplyVolume() { pendingCmd = 200; }   // sent from the render thread

void puppetCommand(const String &cmd) {
  if (cmd == "wake") pendingCmd = 1;
  else if (cmd == "sleep") pendingCmd = 2;
  else if (cmd.startsWith("sound:")) pendingCmd = 100 + constrain(cmd.substring(6).toInt(), 1, 99);
}

String puppetStatusJson() {
  static const char *names[] = {"no radar", "asleep", "waking", "awake"};
  char b[160];
  snprintf(b, sizeof(b), "{\"state\":\"%s\",\"radar\":%s,\"person\":%s,\"x\":%d,\"y\":%d,\"dist\":%d}",
           names[state], radarAlive() ? "true" : "false", lastSomeone ? "true" : "false",
           lastTarget.x, lastTarget.y, (int)lastDist);
  return String(b);
}
