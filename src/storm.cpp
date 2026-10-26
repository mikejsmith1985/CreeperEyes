// storm.cpp -- see storm.h
#include "storm.h"
#include "settings.h"
#include "puppet.h"
#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel strip(STORM_LEDS, STORM_PIN, NEO_GRB + NEO_KHZ800);
static volatile bool wantStrike = false;
static volatile bool striking = false;

// Where each LED sits in the scene (0..1, 0,0 = bottom-left as seen from the front)
static float ledX[STORM_LEDS], ledY[STORM_LEDS];
static void layout() {
  for (int r = 0; r < STORM_ROWS; r++)
    for (int c = 0; c < STORM_PER_ROW; c++) {
      int i = r * STORM_PER_ROW + (r % 2 ? STORM_PER_ROW - 1 - c : c);   // zig-zag
      // strip is stuck on the back wall, so seen from the front left/right is mirrored
      ledX[i] = 1.0f - (c + 0.5f) / STORM_PER_ROW;
      ledY[i] = (r + 0.5f) / STORM_ROWS;
    }
}
// Glow spots, placed by eye on the built box (front view: tree + moon left, house centre-right).
// ledX runs opposite to the front view, so front x ~= 1 - ledX.
static float spot(int i, float x, float y, float sx, float sy) {
  float dx = (ledX[i] - x) / sx, dy = (ledY[i] - y) / sy;
  return expf(-(dx * dx + dy * dy));
}
static float houseW(int i) { return spot(i, 0.46f, 0.45f, 0.11f, 0.16f); }   // the lit windows
static float moonW(int i)  { return spot(i, 0.82f, 0.84f, 0.09f, 0.14f); }   // the moon, top-left

static uint8_t sc(float v) { return (uint8_t)constrain((int)v, 0, 255); }

static void stormTask(void *) {
  layout();
  strip.begin(); strip.clear(); strip.show();
  float flick[STORM_LEDS] = {0};
  uint32_t nextAuto = millis() + 20000;
  for (;;) {
    uint32_t now = millis();
    float glow = gSet.glowBright / 100.0f;                 // 0..1
    float flash = gSet.stormBright / 100.0f * 150.0f;      // max white level (caps the current draw)

    if (gSet.stormAuto && now > nextAuto) { wantStrike = true; }
    if (wantStrike) {
      wantStrike = false; striking = true;
      uint32_t every = max<uint32_t>(20, gSet.stormEveryS) * 1000UL;
      nextAuto = now + every * (70 + esp_random() % 61) / 100;     // +-30 % so it isn't predictable
      // 2-4 flickers; each flash has a centre so the light seems to come from one part of the sky
      int n = 2 + esp_random() % 3;
      float cx = 0.15f + (esp_random() % 70) / 100.0f;
      puppetStormReaction(0);                                       // monster: pupils snap, startled
      for (int k = 0; k < n; k++) {
        float lvl = (k == n - 1 ? 1.0f : 0.45f + (esp_random() % 50) / 100.0f);
        for (int i = 0; i < STORM_LEDS; i++) {
          float d = fabsf(ledX[i] - cx);
          float v = flash * lvl * (0.55f + 0.45f * expf(-d * d / 0.08f)) * (0.6f + 0.4f * ledY[i]);
          strip.setPixelColor(i, sc(v * 0.85f), sc(v * 0.9f), sc(v));         // blue-white
        }
        strip.show();
        vTaskDelay(pdMS_TO_TICKS(40 + esp_random() % 90));
        for (int i = 0; i < STORM_LEDS; i++) strip.setPixelColor(i, 0);
        strip.show();
        vTaskDelay(pdMS_TO_TICKS(50 + esp_random() % 160));
        cx += ((int)(esp_random() % 30) - 15) / 100.0f;
      }
      // after-glow fades, then the thunder rolls in (sound travels slower than light)
      for (int f = 10; f > 0; f--) {
        for (int i = 0; i < STORM_LEDS; i++) { float v = flash * 0.18f * f / 10; strip.setPixelColor(i, sc(v * 0.8f), sc(v * 0.85f), sc(v)); }
        strip.show(); vTaskDelay(pdMS_TO_TICKS(25));
      }
      vTaskDelay(pdMS_TO_TICKS(250 + esp_random() % 900));
      puppetStormReaction(1);                                       // thunder sound
      striking = false;
      continue;
    }

    // ambient: dim stormy blue-purple sky that slowly breathes, candle-flicker amber in the windows
    float breathe = 0.8f + 0.2f * sinf(now / 2300.0f);
    for (int i = 0; i < STORM_LEDS; i++) {
      flick[i] += ((esp_random() % 1000) / 1000.0f - flick[i]) * 0.25f;
      float h = houseW(i), m = moonW(i);
      float sky = glow * breathe * 26.0f * max(0.0f, 1 - h - m);
      float candle = glow * h * (45.0f + 25.0f * flick[i]);
      float moon = glow * m * 75.0f;                                   // pale, slightly green-white
      strip.setPixelColor(i, sc(sky * 0.35f + candle + moon * 0.80f),
                             sc(sky * 0.25f + candle * 0.42f + moon * 1.00f),
                             sc(sky * 1.0f + candle * 0.05f + moon * 0.72f));
    }
    strip.show();
    vTaskDelay(pdMS_TO_TICKS(40));
  }
}

void stormStart() { xTaskCreatePinnedToCore(stormTask, "storm", 4096, nullptr, 1, nullptr, 0); }
void stormStrike() { wantStrike = true; }
bool stormActive() { return striking; }
