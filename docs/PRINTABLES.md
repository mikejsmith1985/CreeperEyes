# Printables listing draft

> Post as a **Remix** of the head model on Printables (use "Add remix source") so the
> credit links automatically. Fill in the two `TODO` items before publishing.

## Title
Frankenstein Bust with Radar-Tracking Animated Eyes (ESP32)

## Summary
A full Frankenstein bust that wakes up when someone walks past, follows them with animated OLED eyes, and groans when they lean in. Built for trunk-or-treat: everything plugs together, no soldering, and the firmware installs from your browser.

## Description
This remix adds a neck-and-shoulders base to Jose Aveleira's Frankenstein head with animated eyes. The base is a hollowed, re-fitted version of TODO-BUST-NAME by TODO-BUST-AUTHOR, cut to sit the head in a tapered socket and hollowed to hold the electronics.

**What it does**
- Two 1.5" OLED eyes animate continuously (Uncanny Eyes).
- A 24 GHz radar hidden behind the shirt sees through the plastic. When someone approaches, the monster wakes up, his eyes follow the nearest person, and he growls.
- Lean in close and he says "Mmm…".
- Set eye style, iris and eyelid colour, brightness, volume and sensitivity from your phone over the monster's own Wi-Fi.

**Build guide and one-click firmware installer:** TODO-PAGES-URL (https://mikejsmith1985.github.io/CreeperEyes/ once Pages is enabled)

## Print settings
| Part | Settings |
|---|---|
| Neck & shoulders base | Upright, no supports, 0.2 mm, 3 walls, 15% gyroid, 5 mm brim. Pre-painted for 3 colours (coat black, shirt brown, neck green). Use plain, non-metallic PLA so the radar can see through the chest. |
| Head, hair, ears, eye sockets, staples | As in the original model. |
| Neck bolts | Remixed bolts included (FrankensteinBolt.3mf). |

Drill a 25 mm hole through the centre of the head's flat neck bottom for the screen cables.

## Electronics (no soldering)
ESP32 DevKit (38-pin) with screw-terminal breakout · 2× Waveshare 1.5" RGB OLED (SSD1351) · HLK-LD2450 radar (with 4-pin cable) · DFPlayer Mini · 3 W 4 Ω speaker · microSD · 1 kΩ resistor · 470 µF capacitor · mini breadboard · USB power bank.

## Credits
- **Frankenstein Head with Animated Eyes**: Jose Aveleira, https://www.printables.com/model/620191-frankenstein-head-with-animated-eyes
- **Bust (neck and shoulders source)**: TODO-BUST-AUTHOR, TODO-BUST-LINK
- **Uncanny Eyes**: Phil Burgess / Paint Your Dragon for Adafruit Industries (MIT)
- **ESP32 dual-eye port**: Laurent Moll
- **CreeperEyes firmware, radar and settings page**: https://github.com/mikejsmith1985/CreeperEyes

## License
Match the most restrictive license of the source models (check both before publishing).
