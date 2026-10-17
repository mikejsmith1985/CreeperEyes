# Radar + sound add-on

## Wiring
| Module | Pin | ESP32 |
|---|---|---|
| LD2450 radar | 5V / GND | 5V / GND |
| | TX | GPIO 32 |
| | RX | GPIO 33 |
| DFPlayer Mini | VCC / GND | 5V / GND (470 uF cap across VCC-GND, stripe to GND) |
| | RX | 1 kOhm resistor -> GPIO 25 |
| | TX | GPIO 26 |
| | SPK_1 / SPK_2 | speaker |

> **DFPlayer orientation:** the pin labels are on the underside, and clones (e.g. HiLetgo HW-247A) have none. Hold it with the
> microSD slot pointing **down**: VCC is then the top-left pin,
> followed by RX, TX, DAC_R, DAC_L, SPK_1, GND, SPK_2. Upside-down, it stays silent and echoes commands back.

## microSD card (FAT32, <= 32 GB)
Create a folder named `mp3` and copy in everything from `sounds/mp3/` (or your own recordings with the same names):
- `0001.mp3` wake-up groan (also plays once at power-on as a test)
- `0002.mp3` .. `0005.mp3` random growls while someone is in front
- `0006.mp3` "Mmm..." when someone leans in close
- `0007.mp3` roar (jump scare, transformation) - falls back to 0002 if missing
- `0008.mp3` snore while asleep - skipped if missing
- `0009.mp3` friendly "hmm?" (kid-friendly mode) - falls back to 0006
- `0010.mp3` chuckle (staring contest, kid-mode lean-in) - falls back to 0006

`tools/monster_sounds.py` regenerates the synthesized set.

## Behaviour
- No radar connected: original UncannyEyes motion plus occasional antics; phone puppet mode still works.
- Nobody around: lids droop to a sleepy sliver, occasional twitch and snore, no blinking. After 20 s the
  power saver dims the screens and slows the frame rate.
- Someone within the wake distance: groan, eyes snap open, pupils dilate, eyes follow them. Counts as a visitor.
- Two or three people: his eyes glance between them every 1-3 s.
- Someone rushing at him (closing faster than 1.3 m/s, within 2.2 m): jump scare - roar, eyes snap wide, pupils pinpoint.
- Someone standing still within 1.8 m for 2.5 s: staring contest - no blinking, pupils slowly narrow, then he
  loses with a big slow blink and a chuckle.
- Someone lingering 20 s: transformation into dragon eyes (goat if already dragon) for 6 s.
- Every 12-25 s while watched: a wink, eye roll or cross-eyed moment. Looks around when someone leaves,
  nods off when falling asleep.
- "New eyes for each visitor": a random eye style each time he wakes.
- Kid-friendly mode: no roar/jump scare/transformation, softer sounds, winks instead of pupil-shrink, 2/3 volume,
  random eyes picked from the friendly set.

All of these have switches on the phone page (Behaviour section).

## Puppet mode (phone)
Drag on the pad to steer the eyes; buttons for winks, slow blink, eyes shut, cross-eyed, eye roll, look around
and sounds. Normal behaviour resumes 90 s after the last touch or on Release.

## Wireless updates
The flash holds two app slots (1.1 MB each) and a separate `eyes` partition with the eye graphics
(`eyedata/eyes.bin`, built by `tools/eyegen.py`). The first install after this change must be over USB
(browser installer or `pio run -t upload`, which also writes the eyes partition). After that:
1. Download `creepereyes-update.bin` to your phone (installer site or the CI artifact).
2. Join the monster's Wi-Fi, open the page, Firmware update -> pick the file -> Install.
3. He restarts with the new firmware in about a minute. Settings and the visitor count are kept.

## Tuning
- Flip left/right or up/down on the phone page if the eyes look the wrong way.
- Distances, sleep delay and volume are on the phone page.
- `RADAR_DEBUG_PRINT 1` in include/puppet.h prints the nearest target, closing speed and people count.
