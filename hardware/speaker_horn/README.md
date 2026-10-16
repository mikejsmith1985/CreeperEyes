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
Speaker sits 20 mm behind the wall, 14 mm above the grill centre (clears the base).

Rebuild: `geo.py` (grill plane) -> `horn4.py` (voxel shell + placement search) -> `build.py`.
