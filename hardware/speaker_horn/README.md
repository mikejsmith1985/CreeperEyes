# Speaker horn for the bust grill

Holds a 40 mm speaker (Gikfun EK1794, 4 ohm 3 W) behind the jacket grill and channels its sound
straight out of the grill holes. The grill end is shaped to the inside wall of the bust
(`CreeperEyes_NeckShoulders_paintready.stl`), so it seals all round the holes.

- `CreeperEyes_SpeakerHorn.stl` - print this (already oriented: flat speaker end on the bed)
- `CreeperEyes_SpeakerFitTest.stl` - 5-minute ring to check the speaker fit first
- `speaker_horn_in_bust_position.stl` - same part in the bust's coordinates, for checking in a slicer/CAD

Prints with no supports: every overhang is 45 deg or steeper (flange underside and throat are
chamfered, the wire slot has a pointed roof). The only flat bit is a 0.5 mm speaker seat ledge.
The glue face is smoothed to match the bust wall.

Pocket: 40.7 mm with three crush ribs, 7 mm deep, slot at the top for the solder tabs.
v3: placed against the real printed bust (including the 45 deg shoulder slab inside, which v2 hit).
Speaker sits 22 mm behind the wall, 15 mm above and 9 mm towards the centre from the grill centre.
Fit it by lowering it in through the open base about 20 mm back from the wall, then pushing it straight onto the grill.

Rebuild: `geo.py` (grill plane) -> `place.py` (placement search against the bust solid, collision + overhang) -> `build.py`.
