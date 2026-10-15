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
Create a folder named `mp3` and add:
- `0001.mp3` wake-up groan (also plays once at power-on as a test)
- `0002.mp3` .. `0005.mp3` random growls while someone is in front
- `0006.mp3` "Mmm..." when someone leans in close

## Behaviour
- No radar connected: eyes behave exactly like the original UncannyEyes.
- Nobody around: lids droop to a sleepy sliver, occasional twitch, no blinking.
- Someone within 2.5 m: groan, eyes snap open, pupils dilate, eyes follow the nearest person.
- Someone closer than 45 cm: "Mmm...", pupils shrink.
- Nobody for 8 s: slowly falls back asleep.

## Tuning (include/puppet.h)
- `GAZE_DIR_X` / `GAZE_DIR_Y`: flip to -1 if the eyes look the wrong way.
- `WAKE_DISTANCE_MM`, `LEAN_DISTANCE_MM`, `SLEEP_AFTER_MS`, `SOUND_VOLUME`.
- Serial monitor at 921600 baud prints the nearest radar target twice a second.
