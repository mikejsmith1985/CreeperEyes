# Storm backdrop (Prusa XL)

Layered shadow-box behind the bust: a lithophane storm sky (moon, clouds, bats, lightning bolt),
then a crooked house on a hill, a dead tree with a crow, and a graveyard with an iron fence.
WS2812 LEDs in a light box behind it glow stormy blue with candle-lit windows, then flash for lightning,
followed by thunder from the monster's speaker.

All parts are 330 x 330 mm or smaller, print flat, no supports.

| Part | Qty | Filament | Notes |
|---|---|---|---|
| 0_led_light_box | 1 | white | open side up; LED channels on the floor |
| 1_sky_lithophane | 1 | white | flat face DOWN, 100 % infill, 0.12-0.15 mm layers |
| 2_house_and_hill | 1 | dark purple/grey (or black) | |
| 3_dead_tree | 1 | black | |
| 4_graves_and_fence | 1 | black | |
| 5_front_bezel | 1 | black | chamfered face up |
| spacer_ring_x3 | 3 | black | |
| 6_bust_plinth | 1 | black | bust drops into the pocket |
| 7_backdrop_foot_x2 | 2 | black | the stack stands in these |

Stack, back to front: light box -> lithophane (flat face toward the viewer) -> ring -> house
-> ring -> tree -> ring -> graves -> bezel. Corner holes take 1.75 mm filament as alignment pins.

LEDs: 1 m WS2812B 60/m. 3 rows of 18 (54 LEDs, 300 mm per row) in the channels, zig-zag, starting bottom-left as seen from the
BACK (wire exit). Data -> 330 ohm -> GPIO27. 5 V + GND to the power bank rail, 470 uF across them.
