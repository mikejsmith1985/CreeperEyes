# PlatformIO post-script: flash eyedata/eyes.bin into the 'eyes' partition alongside the app,
# so a normal "pio run -t upload" leaves the board complete.
Import("env")
import os
eyes = os.path.join(env.subst("$PROJECT_DIR"), "eyedata", "eyes.bin")
if os.path.exists(eyes):
    env.Append(UPLOADERFLAGS=["0x250000", eyes])
else:
    print("WARNING: eyedata/eyes.bin missing - run tools/eyegen.py; eyes will not display")
