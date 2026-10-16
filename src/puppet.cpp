// puppet.cpp -- see include/puppet.h for what this does and how it is wired.
#include "puppet.h"
void webuiSerialSet(const char *kv);   // webui.cpp
#include "settings.h"
#include "eyestyle.h"
#include "soundcfg.h"
#include <HardwareSerial.h>
#include <Preferences.h>
#include <math.h>

// The iris range used by main.cpp (kept in sync with IRIS_MIN / IRIS_MAX there).
static const int16_t kIrisMin = 150, kIrisMax = 400;

static HardwareSerial radarSerial(2);
static HardwareSerial soundSerial(1);

// Elapsed ms, never negative (timestamps set via millis() or by the web task can be newer than 'now').
static inline uint32_t since(uint32_t now, uint32_t t) { return (int32_t)(now - t) < 0 ? 0 : now - t; }

static inline float frand(float a, float b) { return a + (b - a) * (random(10000) / 10000.0f); }

// =====================================================================
// LD2450 radar: 30-byte frames  AA FF 03 00 | 3 x 8-byte targets | 55 CC
// Each target: x, y, speed (int16 where bit15 set = positive), resolution.
// Units are millimetres; y is distance straight out from the radar.
// =====================================================================
struct Target { bool valid; int16_t x, y; };
static Target   targets[3];
static uint32_t lastRadarFrameMs = 0, radarFrames = 0;
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
  radarFrames++;
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

static float distOf(const Target &t);
static bool inZone(const Target &t) { return t.valid && t.y > 0 && abs(t.x) <= ZONE_HALF_WIDTH_MM && distOf(t) <= gSet.rangeMm; }
static float distOf(const Target &t) { return sqrtf((float)t.x * t.x + (float)t.y * t.y); }

// Nearest person inside the zone in front of the bust. Returns slot index or -1.
static int nearestTarget(float &distMm) {
  int best = -1; distMm = 1e9;
  for (int i = 0; i < 3; i++) {
    if (!inZone(targets[i])) continue;
    float d = distOf(targets[i]);
    if (d < distMm) { distMm = d; best = i; }
  }
  return best;
}

// =====================================================================
// DFPlayer Mini: 10-byte commands, no library needed.
// Command 0x12 plays /mp3/NNNN.mp3 by its number (not by copy order).
// =====================================================================
static uint32_t soundBusyUntilMs = 0;
static uint16_t lastTrack = 0;
static uint32_t lastTrackMs = 0;

static void dfSend(uint8_t cmd, uint16_t param) {
  uint8_t m[10] = {0x7E, 0xFF, 0x06, cmd, 0x00, (uint8_t)(param >> 8), (uint8_t)param, 0, 0, 0xEF};
  uint16_t sum = 0;
  for (int i = 1; i < 7; i++) sum += m[i];
  sum = -sum;
  m[7] = sum >> 8; m[8] = sum & 0xFF;
  soundSerial.write(m, 10);
}

static void playTrack(uint16_t n, uint32_t lengthMs, bool force = false);

// Optional newer tracks fall back to the original six if they are not on the card.
static uint16_t fallbackFor(uint16_t n) {
  switch (n) { case TRK_ROAR: return soundRoleIsDefault(R_ROAR) ? TRK_GROWL_FIRST : 0;
               case TRK_HMM: return soundRoleIsDefault(R_HMM) ? TRK_LEAN : 0;
               case TRK_CHUCKLE: return soundRoleIsDefault(R_CHUCKLE) ? TRK_LEAN : 0; default: return 0; }
}
static uint16_t dfFiles = 0;
uint16_t puppetFilesOnCard() { return dfFiles; }

// Read whatever the DFPlayer says back; log it, and retry missing files with a fallback.
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
    if (cmd == 0x40 && p == 6 && millis() - lastTrackMs < 1500) {      // file not found
      uint16_t fb = fallbackFor(lastTrack);
      Serial.printf("[sound] track %u missing on card%s\n", lastTrack, fb ? ", using fallback" : "");
      if (fb) playTrack(fb, 2500, true);
    } else if (cmd == 0x48) {
      dfFiles = p;
      Serial.printf("[sound] DFPlayer ready, %u files on card\n", p);
    } else if (cmd == 0x40) {
      Serial.printf("[sound] DFPlayer error %u\n", p);
    }
  }
}

void puppetEarly() {   // start listening to the DFPlayer as early as possible
  soundSerial.begin(SOUND_BAUD, SERIAL_8N1, SOUND_RX_PIN, SOUND_TX_PIN);
}

static void playTrack(uint16_t n, uint32_t lengthMs, bool force) {
  uint32_t now = millis();
  if (n == 0) return;                          // role left empty on purpose: stay silent
  if (!force && now < soundBusyUntilMs) return;
  dfSend(0x12, n);
  lastTrack = n; lastTrackMs = now;
  soundBusyUntilMs = now + lengthMs;
  Serial.printf("[sound] track %u\n", n);
}

static uint8_t effectiveVolume() { return gSet.kidMode ? (gSet.volume * 2) / 3 : gSet.volume; }

// =====================================================================
// Visitor counter (tonight = since power-on, total = saved in flash)
// =====================================================================
static Preferences visitPrefs;
static volatile uint32_t visitsTonight = 0, visitsTotal = 0;
static volatile bool     visitsDirty = false;
static uint32_t visitsSavedMs = 0;
static uint32_t zoneEmptySinceMs = 0;
static bool     zonePresent = false;

void puppetResetVisitsTonight() { visitsTonight = 0; }
void puppetSaveVisitsIfNeeded() {
  if (!visitsDirty || millis() - visitsSavedMs < 20000) return;
  visitsDirty = false; visitsSavedMs = millis();
  visitPrefs.begin("creeperv", false); visitPrefs.putUInt("total", visitsTotal); visitPrefs.end();
}

// =====================================================================
// Antics: short scripted eye moves, per eye (winks, rolls, cross-eyes...)
// =====================================================================
enum Antic : uint8_t { AN_NONE, AN_WINK_L, AN_WINK_R, AN_ROLL, AN_CROSS, AN_LOOK, AN_BLINK_SLOW, AN_NOD };
static Antic    antic = AN_NONE;
static uint32_t anticStartMs = 0, anticDurMs = 0;

static void startAntic(Antic a) {
  static const uint16_t dur[] = {0, 500, 500, 1600, 2600, 2000, 1700, 2600};
  antic = a; anticStartMs = millis(); anticDurMs = dur[a];
  Serial.printf("[puppet] antic %u (%u ms)\n", a, anticDurMs);
}

// Adds this antic's offsets for one eye. Returns false when no antic is running.
static bool evalAntic(uint8_t eye, uint32_t now, float &dx, float &dy, float &lidE) {
  dx = dy = lidE = 0;
  if (antic == AN_NONE) return false;
  float f = since(now, anticStartMs) / (float)anticDurMs;
  if (f >= 1) { antic = AN_NONE; Serial.printf("[puppet] antic done after %u ms\n", since(now, anticStartMs)); return false; }
  float env = fminf(1.0f, fminf(f * 4, (1 - f) * 4));            // ease in / out
  switch (antic) {
    case AN_WINK_L: if (eye == 0) lidE = 255 * sinf(PI * f); break;
    case AN_WINK_R: if (eye == 1) lidE = 255 * sinf(PI * f); break;
    case AN_ROLL:   dx = 470 * sinf(2 * PI * f) * env; dy = 470 * cosf(2 * PI * f) * env; break;
    case AN_CROSS:  dx = (eye == 0 ? 470 : -470) * env; break;
    case AN_LOOK:   dx = 480 * sinf(2 * PI * f) * env; break;
    case AN_BLINK_SLOW:
      lidE = f < 0.35f ? 255 * f / 0.35f : f < 0.6f ? 255 : 255 * (1 - f) / 0.4f; break;
    case AN_NOD:                                                  // nodding off, then jerk awake
      if (f < 0.8f) { lidE = 225 * f / 0.8f; dy = -120 * f; }
      else { lidE = 0; dy = 80 * sinf((f - 0.8f) * 40); }
      break;
    default: break;
  }
  return true;
}

static bool anticClosesLids() { return antic == AN_WINK_L || antic == AN_WINK_R || antic == AN_BLINK_SLOW || antic == AN_NOD; }

// =====================================================================
// Behaviour state machine
// =====================================================================
enum State { NO_RADAR, ASLEEP, WAKING, AWAKE };
static State    state = NO_RADAR;
static uint32_t stateSinceMs = 0, lastSeenMs = 0, nextGrowlMs = 0, leanCooldownMs = 0, nextTwitchMs = 0;
static uint32_t nextAnticMs = 0, nextSnoreMs = 0;
static float    lid = 0;            // shared lid closure 0..255
static float    lidTarget = 0;
static float    gazeX = 512, gazeY = 512, gazeTX = 512, gazeTY = 512, gazeRate = 6;
static int16_t  irisOverride = -1;
static uint32_t irisUntilMs = 0;
static uint32_t lastUpdateMs = 0;
static volatile int pendingCmd = 0;     // 1 wake, 2 sleep, 1000+N play sound N
static volatile bool pendingVolume = false;
static Target   lastTarget = {false, 0, 0};
static float    lastDist = 0;
static bool     lastSomeone = false;
static int      peopleInZone = 0;

// Output, per eye
static float    outGX[2] = {512, 512}, outGY[2] = {512, 512}, outLid[2] = {0, 0};
static bool     outGazeValid = false;

// Group watching
static int      focusSlot = -1;
static uint32_t nextFocusSwitchMs = 0;

// Jump scare (closing speed of the nearest person)
static float    histD[8]; static uint32_t histT[8]; static int histN = 0, histSlot = -1;
static int16_t  histX = 0; static uint32_t histFrame = 0;
static uint32_t scareCooldownMs = 0;

// Staring contest
static bool     staring = false;
static int16_t  stareAX = 0, stareAY = 0;
static uint32_t stareAnchorMs = 0, stareStartMs = 0, stareLenMs = 0, stareCooldownMs = 0;

// Transformation + random eyes per visitor
static uint32_t lingerStartMs = 0, transformEndMs = 0, transformCooldownMs = 0;
static int16_t  transformStyle = -1, visitStyle = -1;
static uint16_t styleVersion = 0;

// Phone puppet mode
static volatile int16_t  phoneX = -1, phoneY = -1;
static volatile uint32_t phoneLastMs = 0;
static volatile bool     phoneActive = false;
static bool              phoneClosed = false;
static volatile uint8_t  actQ[8]; static volatile uint8_t actHead = 0, actTail = 0;

// Power saver
static bool     saving = false;

static void setState(State s) {
  static const char *names[] = {"NO_RADAR", "ASLEEP", "WAKING", "AWAKE"};
  state = s; stateSinceMs = millis();
  Serial.printf("[puppet] -> %s\n", names[s]);
}

static void setVisitStyle(int16_t s) { if (visitStyle != s) { visitStyle = s; styleVersion++; } }
static void setTransformStyle(int16_t s) { if (transformStyle != s) { transformStyle = s; styleVersion++; } }

static int16_t pickRandomStyle() {
  if (!kEyeStyleCount) return -1;
  static const char *friendly[] = {"Human", "Cat", "Owl", "Googly monster", "Fish"};
  int16_t cur = gSet.eye[0].style, pick = -1;
  for (int tries = 0; tries < 8; tries++) {
    if (gSet.kidMode) pick = eyeStyleFind(friendly[random(5)]);
    else              pick = random(kEyeStyleCount);
    if (pick >= 0 && pick != cur) break;
  }
  return pick;
}

void puppetSetup() {
  soundCfgLoad();
  visitPrefs.begin("creeperv", true); visitsTotal = visitPrefs.getUInt("total", 0); visitPrefs.end();
  radarSerial.begin(RADAR_BAUD, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
  for (uint32_t t0 = millis(); millis() - t0 < 1500; ) { pollDf(); delay(20); }   // DFPlayer boot time
  dfSend(0x06, effectiveVolume());   // set volume
  delay(100);
  dfSend(0x48, 0);                   // ask how many files are on the card (logged when it answers)
  for (uint32_t t0 = millis(); millis() - t0 < 300; ) { pollDf(); delay(10); }
  playTrack(soundForRole(gSet.kidMode ? R_HMM : R_WAKE), 3000, true);   // boot test
  Serial.printf("[puppet] started. %u visitors so far all-time\n", visitsTotal);
}

static float approach(float cur, float target, float rate, float dtS) {
  float k = 1.0f - expf(-rate * dtS);
  return cur + (target - cur) * k;
}

static void gazeAt(const Target &t, float dist) {
  float ang = atan2f((float)t.x, (float)t.y);                  // radians, 0 = straight ahead
  float gx = 512 + (gSet.flipGazeX ? -1 : 1) * (ang / 0.79f) * 470;   // +-45 deg -> +-470 (full swing)
  float down = constrain((2000.0f - dist) / 1600.0f, 0.0f, 1.0f);  // closer kids -> look down
  float gy = 512 - (gSet.flipGazeY ? -1 : 1) * down * 300;
  gazeTX = constrain(gx, 40.0f, 983.0f);
  gazeTY = constrain(gy, 80.0f, 943.0f);
}

static void handlePhoneAction(uint8_t a, uint32_t now) {
  Serial.printf("[puppet] phone action %u\n", a);
  switch (a) {
    case PA_WINK_L: startAntic(AN_WINK_L); break;
    case PA_WINK_R: startAntic(AN_WINK_R); break;
    case PA_BLINK:  startAntic(AN_BLINK_SLOW); break;
    case PA_CROSS:  startAntic(AN_CROSS); gazeTX = gazeX = 512; gazeTY = gazeY = 512; break;
    case PA_ROLL:   startAntic(AN_ROLL); break;
    case PA_LOOK:   startAntic(AN_LOOK); break;
    case PA_CLOSE_TOGGLE: phoneClosed = !phoneClosed; break;
    case PA_RELEASE: phoneActive = false; phoneClosed = false; Serial.println("[puppet] phone released"); break;
    default:
      if (a >= 100) playTrack(a - 100, 1000, true);
      break;
  }
}

void puppetUpdate(uint32_t nowMicros) {
  (void)nowMicros;
  uint32_t now = millis();
  float dt = lastUpdateMs ? since(now, lastUpdateMs) / 1000.0f : 0.0f;
  lastUpdateMs = now;

  pollRadar();
  pollDf();

  int cmd = pendingCmd; pendingCmd = 0;
  if (pendingVolume) { pendingVolume = false; dfSend(0x06, effectiveVolume()); }
  if (cmd >= 1000) playTrack(cmd - 1000, 1000, true);

  // Serial test hooks: "a<N>" fires phone action N, "s<key>=<value>" changes a setting
  static char sbuf[48]; static uint8_t sn = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') { sbuf[sn] = 0; if (sn > 1 && sbuf[0] == 'a') puppetPhone(-1, -1, atoi(sbuf + 1));
      else if (sn > 2 && sbuf[0] == 's') webuiSerialSet(sbuf + 1);
      sn = 0; }
    else if (sn < sizeof(sbuf) - 1) sbuf[sn++] = c;
  }
  // Phone actions queued by the web task
  while (actTail != actHead) { uint8_t a = actQ[actTail]; actTail = (actTail + 1) & 7; handlePhoneAction(a, now); }
  if (phoneActive && since(now, phoneLastMs) > 90000) { phoneActive = false; phoneClosed = false; Serial.println("[puppet] phone idle, back to normal"); }

  // ---------------- what the radar sees ----------------
  bool alive = radarAlive();
  float dist = 0; int ns = alive ? nearestTarget(dist) : -1;
  bool someone = ns >= 0;
  Target t = someone ? targets[ns] : Target{false, 0, 0};
  if (someone) lastSeenMs = now;
  lastSomeone = someone; if (someone) { lastTarget = t; lastDist = dist; }
  peopleInZone = 0; if (alive) for (auto &tt : targets) if (inZone(tt)) peopleInZone++;

  // Visitor counter: someone arrives within wake distance after the zone was empty for 4 s
  bool present = someone && dist < gSet.wakeDistMm;
  if (present && !zonePresent && since(now, zoneEmptySinceMs) > 4000) {
    visitsTonight++; visitsTotal++; visitsDirty = true;
    Serial.printf("[puppet] visitor #%u tonight\n", visitsTonight);
  }
  if (present) zonePresent = true;
  else if (zonePresent) { zonePresent = false; zoneEmptySinceMs = now; }

  // Closing speed of the nearest person (only on fresh radar frames, same person)
  float closing = 0;
  if (someone && radarFrames != histFrame) {
    histFrame = radarFrames;
    if (ns != histSlot || abs(t.x - histX) > 500) histN = 0;      // different person -> restart
    histSlot = ns; histX = t.x;
    if (histN == 8) { memmove(histD, histD + 1, 7 * sizeof(float)); memmove(histT, histT + 1, 7 * sizeof(uint32_t)); histN = 7; }
    histD[histN] = dist; histT[histN] = now; histN++;
  } else if (!someone) histN = 0;
  for (int i = 0; i < histN - 1; i++) {                           // look back ~300-800 ms
    uint32_t span = histT[histN - 1] - histT[i];
    if (span >= 300 && span <= 900) { closing = (histD[i] - histD[histN - 1]) * 1000.0f / span; break; }
  }

  if (!alive) {                      // no radar -> original behaviour (+ phone and antics)
    if (state != NO_RADAR) setState(NO_RADAR);
    lidTarget = 0; irisOverride = -1; staring = false;
    setVisitStyle(-1); setTransformStyle(-1);
  } else {
    if (state == NO_RADAR) { setState(ASLEEP); nextTwitchMs = now + 5000; nextSnoreMs = now + 30000; }
    if (cmd == 1 && state == ASLEEP) { someone = true; dist = 1000; t = {true, 0, 1000}; lastSeenMs = now; }
    if (cmd == 2 && state != ASLEEP) setState(ASLEEP);
  }

#if RADAR_DEBUG_PRINT
  static uint32_t lastPrint = 0;
  if (now - lastPrint > 500) {
    lastPrint = now;
    if (someone) Serial.printf("[radar] nearest x=%d y=%d dist=%.0f closing=%.0f people=%d\n", t.x, t.y, dist, closing, peopleInZone);
    else         Serial.println("[radar] nobody in zone");
  }
#endif

  bool phone = phoneActive;
  bool scary = !gSet.kidMode;

  // ---------------- jump scare (works asleep or awake) ----------------
  if (alive && !phone && scary && gSet.jumpScares && someone && dist < min(2200, (int)gSet.rangeMm) && closing > 1100 && now > scareCooldownMs) {
    Serial.printf("[puppet] JUMP SCARE (closing at %.0f mm/s)\n", closing);
    if (state == ASLEEP) {
      setState(AWAKE); lingerStartMs = now; nextAnticMs = now + random(8000, 15000);
      if (gSet.randomEyes) setVisitStyle(pickRandomStyle());
    }
    lid = 0; lidTarget = 0; antic = AN_NONE; staring = false;
    irisOverride = kIrisMin; irisUntilMs = now + 1500;
    playTrack(soundForRole(R_ROAR), 3000, true);
    gazeAt(t, dist); gazeX = gazeTX; gazeY = gazeTY;               // snap straight to them
    scareCooldownMs = now + 25000; nextGrowlMs = now + random(20000, 35000);
  }

  // ---------------- behaviour ----------------
  if (alive) switch (state) {
    case ASLEEP:
      lidTarget = 238;               // a sleepy sliver
      gazeTX = 512; gazeTY = 512 - 120 * (gSet.flipGazeY ? -1 : 1);     // eyes rolled down a bit
      staring = false; setTransformStyle(-1); setVisitStyle(-1);
      if (now > nextTwitchMs) {      // occasional twitch while sleeping
        lid = 190;
        nextTwitchMs = now + random(8000, 20000);
      }
      if (gSet.sleepSounds && !phone && now > nextSnoreMs) {
        playTrack(soundForRole(R_SNORE), 3000);
        nextSnoreMs = now + random(40000, 90000);
      }
      if (someone && dist < gSet.wakeDistMm) {
        setState(WAKING);
        if (!phone) playTrack(soundForRole(gSet.kidMode ? R_HMM : R_WAKE), 3000, true);
        irisOverride = kIrisMax; irisUntilMs = now + 1500;   // pupils snap wide
        lingerStartMs = now; nextAnticMs = now + random(8000, 15000);
        if (gSet.randomEyes) setVisitStyle(pickRandomStyle());
      }
      break;

    case WAKING:
      lidTarget = 0;
      if (since(now, stateSinceMs) > 700) { setState(AWAKE); nextGrowlMs = now + random(15000, 30000); }
      // fall through so gaze follows during wake-up
    case AWAKE: {
      lidTarget = 0;
      if (someone) {
        // Group watching: with 2-3 people, glance between them every second or three.
        if (peopleInZone >= 2 && !staring) {
          if (focusSlot < 0 || !inZone(targets[focusSlot]) || now > nextFocusSwitchMs) {
            int tries = 0;
            do { focusSlot = random(3); } while (!inZone(targets[focusSlot]) && ++tries < 10);
            nextFocusSwitchMs = now + random(1200, 3500);
          }
          const Target &ft = targets[focusSlot];
          gazeAt(ft, distOf(ft));
          gazeRate = 14;             // quick saccade between faces
        } else {
          focusSlot = -1;
          gazeAt(t, dist);
          gazeRate = staring ? 10 : 6;
        }

        // Staring contest: someone standing still close by
        if (!staring && !phone && dist < min(1600, (int)gSet.rangeMm) && now > stareCooldownMs && peopleInZone == 1) {
          if (abs(t.x - stareAX) > 120 || abs(t.y - stareAY) > 120) { stareAX = t.x; stareAY = t.y; stareAnchorMs = now; }
          else if (since(now, stareAnchorMs) > 2500) {
            staring = true; stareStartMs = now; stareLenMs = random(5000, 8000);
            antic = AN_NONE;
            Serial.println("[puppet] staring contest!");
          }
        }
        if (staring) {
          if (abs(t.x - stareAX) > 250 || abs(t.y - stareAY) > 250) { staring = false; stareCooldownMs = now + 8000; }
          else {
            float k = since(now, stareStartMs) / (float)stareLenMs;      // pupils slowly narrow
            irisOverride = kIrisMax - (kIrisMax - kIrisMin) * constrain(k, 0.0f, 1.0f); irisUntilMs = now + 200;
            if (since(now, stareStartMs) > stareLenMs) {                    // he loses: big slow blink
              staring = false; stareCooldownMs = now + 20000;
              startAntic(AN_BLINK_SLOW);
              playTrack(soundForRole(R_CHUCKLE), 2500, true);
            }
          }
        }

        // Leaning in
        if (!phone && dist < gSet.leanDistMm && now > leanCooldownMs && !staring) {
          if (gSet.kidMode) { startAntic(random(2) ? AN_WINK_L : AN_WINK_R); playTrack(soundForRole(R_CHUCKLE), 2500, true); }
          else { playTrack(soundForRole(R_LEAN), 2500, true); irisOverride = kIrisMin; irisUntilMs = now + 2000; }
          lid = 0;
          leanCooldownMs = now + 6000;
        }

        // Growls / friendly noises now and then
        if (!phone && now > nextGrowlMs && !staring) {
          if (gSet.kidMode) { static const SoundRole soft[] = {R_HMM, R_CHUCKLE, R_LEAN}; playTrack(soundForRole(soft[random(3)]), 3000); }
          else playTrack(soundForRole(R_GROWL), 3500);
          nextGrowlMs = now + random(20000, 40000);
        }

        // Transformation when someone lingers
        if (!phone && scary && gSet.transforms && transformStyle < 0 && !staring &&
            since(now, lingerStartMs) > 20000 && now > transformCooldownMs) {
          int16_t cur = visitStyle >= 0 ? visitStyle : gSet.eye[0].style;
          int16_t d = eyeStyleFind("Dragon");
          if (d == cur) d = eyeStyleFind("Goat / Krampus");
          if (d >= 0) {
            setTransformStyle(d); transformEndMs = now + 6000; transformCooldownMs = now + 60000;
            irisOverride = kIrisMin; irisUntilMs = now + 1200;
            playTrack(soundForRole(R_ROAR), 3000, true);
            Serial.println("[puppet] transformation!");
          }
        }

        // Antics while someone watches
        if (!phone && gSet.antics && !staring && antic == AN_NONE && now > nextAnticMs) {
          static const Antic pool[] = {AN_WINK_L, AN_WINK_R, AN_ROLL, AN_CROSS};
          static const Antic kidPool[] = {AN_WINK_L, AN_WINK_R, AN_WINK_L, AN_ROLL};
          startAntic(gSet.kidMode ? kidPool[random(4)] : pool[random(4)]);
          nextAnticMs = now + random(12000, 25000);
        }
      } else {
        staring = false; focusSlot = -1;
        lingerStartMs = now;
        // Looking around for whoever just left
        if (!phone && gSet.antics && antic == AN_NONE && since(now, lastSeenMs) > 1500 && now > nextAnticMs) {
          startAntic(AN_LOOK); nextAnticMs = now + random(6000, 10000);
        }
      }
      if (transformStyle >= 0 && now > transformEndMs) { setTransformStyle(-1); irisOverride = kIrisMax; irisUntilMs = now + 800; }
      if (state == AWAKE && since(now, lastSeenMs) > (gSet.sleepAfterS * 1000UL)) {
        setState(ASLEEP);
        if (gSet.antics && !phone) startAntic(AN_NOD);
        nextSnoreMs = now + random(15000, 30000);
      }
      break;
    }
    default: break;
  }

  // Idle antics without a radar (keeps the original autonomous look lively)
  if (!alive && !phone && gSet.antics && antic == AN_NONE && now > nextAnticMs) {
    static const Antic pool[] = {AN_WINK_L, AN_WINK_R, AN_ROLL, AN_CROSS, AN_LOOK};
    startAntic(pool[random(5)]);
    nextAnticMs = now + random(10000, 22000);
  }

  if (irisOverride >= 0 && now > irisUntilMs) irisOverride = -1;

  // ---------------- phone puppet overrides ----------------
  if (phone) {
    if (phoneX >= 0) { gazeTX = phoneX; gazeTY = phoneY; }
    gazeRate = 16;
    lidTarget = phoneClosed ? 255 : 0;
  }

  // ---------------- smoothing + per-eye output ----------------
  float lidRate = (state == ASLEEP && !phone) ? 1.2f : 10.0f;   // fall asleep slowly, wake fast
  lid   = approach(lid, lidTarget, lidRate, dt);
  gazeX = approach(gazeX, gazeTX, gazeRate, dt);
  gazeY = approach(gazeY, gazeTY, gazeRate, dt);

  bool baseValid = alive || phone;
  bool anyAntic = false;
  for (uint8_t e = 0; e < 2; e++) {
    float dx, dy, le;
    bool a = evalAntic(e, now, dx, dy, le);
    anyAntic |= a;
    float bx = baseValid ? gazeX : 512, by = baseValid ? gazeY : 512;
    outGX[e] = bx + dx; outGY[e] = by + dy;
    outLid[e] = fmaxf(lid, le);
  }
  outGazeValid = baseValid || (anyAntic && (antic == AN_ROLL || antic == AN_CROSS || antic == AN_LOOK || antic == AN_NOD));

  // ---------------- power saver ----------------
  saving = gSet.powerSaver && alive && !phone && state == ASLEEP && since(now, stateSinceMs) > 20000;
}

bool puppetGaze(uint8_t eye, int16_t *x, int16_t *y) {
  if (!outGazeValid || eye > 1) return false;
  float dx = outGX[eye] - 512, dy = outGY[eye] - 512, r = sqrtf(dx * dx + dy * dy);
  if (r > 480) { dx *= 480 / r; dy *= 480 / r; }                 // stay inside the eye's circle
  *x = (int16_t)(512 + dx); *y = (int16_t)(512 + dy);
  return true;
}

uint8_t  puppetLidClose(uint8_t eye) { return (uint8_t)constrain(outLid[eye > 1 ? 1 : eye], 0.0f, 255.0f); }
int16_t  puppetIris()       { return irisOverride; }
bool     puppetAllowBlink() { return state != ASLEEP && !staring && !anticClosesLids() && !(phoneActive && phoneClosed); }
int16_t  puppetStyle(uint8_t eye) { (void)eye; return transformStyle >= 0 ? transformStyle : visitStyle; }
uint16_t puppetStyleVersion() { return styleVersion; }
uint8_t  puppetBrightness() { return saving ? max(1, gSet.brightness / 4) : gSet.brightness; }
uint8_t  puppetFrameDelayMs() { return saving ? 35 : 0; }

void puppetApplyVolume() { pendingVolume = true; }   // sent from the render thread

void puppetCommand(const String &cmd) {
  if (cmd == "wake") pendingCmd = 1;
  else if (cmd == "sleep") pendingCmd = 2;
  else if (cmd.startsWith("sound:")) pendingCmd = 1000 + constrain(cmd.substring(6).toInt(), 1, 2999);
}

void puppetPhone(int x, int y, int action) {
  phoneLastMs = millis();
  if (action != PA_RELEASE) phoneActive = true;
  if (x >= 0 && y >= 0) {                  // same direction fixes as the radar gaze
    x = constrain(x, 0, 1023); y = constrain(y, 0, 1023);
    phoneX = gSet.flipGazeX ? 1023 - x : x;
    phoneY = gSet.flipGazeY ? 1023 - y : y;
  }
  if (action > 0) {
    uint8_t next = (actHead + 1) & 7;
    if (next != actTail) { actQ[actHead] = (uint8_t)action; actHead = next; }
  }
}

String puppetStatusJson() {
  static const char *names[] = {"no radar", "asleep", "waking", "awake"};
  const char *st = phoneActive ? "puppeted" : staring ? "staring" : transformStyle >= 0 ? "transformed" : names[state];
  char b[420];
  int n = snprintf(b, sizeof(b),
    "{\"state\":\"%s\",\"radar\":%s,\"person\":%s,\"people\":%d,\"x\":%d,\"y\":%d,\"dist\":%d,"
    "\"visitsTonight\":%u,\"visitsTotal\":%u,\"puppet\":%s,\"saving\":%s,\"gx\":%d,\"gy\":%d,\"t\":[",
    st, radarAlive() ? "true" : "false", lastSomeone ? "true" : "false", peopleInZone,
    lastTarget.x, lastTarget.y, (int)lastDist, visitsTonight, visitsTotal,
    phoneActive ? "true" : "false", saving ? "true" : "false", (int)outGX[0], (int)outGY[0]);
  bool first = true;
  for (auto &t : targets) if (t.valid && radarAlive()) {
    n += snprintf(b + n, sizeof(b) - n, "%s[%d,%d,%d]", first ? "" : ",", t.x, t.y, inZone(t) ? 1 : 0);
    first = false;
  }
  snprintf(b + n, sizeof(b) - n, "]}");
  return String(b);
}
