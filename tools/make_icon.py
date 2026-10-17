#!/usr/bin/env python3
"""Draws the phone home-screen icon (180x180) and writes src/icon_png.cpp.
An original design: one glowing monster eye under a heavy stitched brow on green skin.
Usage: python3 tools/make_icon.py [preview.png]"""
import io, math, os, sys
import numpy as np
from PIL import Image, ImageFilter

S = 720                      # draw at 4x, scale down to 180 for smooth edges
rng = np.random.default_rng(7)
y, x = np.mgrid[0:S, 0:S].astype(np.float32) / S          # 0..1
img = np.zeros((S, S, 3), np.float32)

def mix(a, b, t):
    t = np.clip(t, 0, 1)[..., None]
    return a * (1 - t) + b * t

def col(h): return np.array([int(h[i:i + 2], 16) for i in (1, 3, 5)], np.float32)

def smooth(e0, e1, v):
    t = np.clip((v - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t)

# ---- skin: green, lit from the top left, mottled ----
noise = Image.fromarray((rng.random((90, 90)) * 255).astype(np.uint8)).resize((S, S), Image.BICUBIC)
noise = np.asarray(noise.filter(ImageFilter.GaussianBlur(6)), np.float32) / 255
light = 1.05 - 0.55 * np.hypot(x - 0.28, y - 0.18)
skin = mix(col('#2c4a26'), col('#7fae5c'), light + (noise - 0.5) * 0.25)
img[:] = skin

# ---- eye geometry (almond) ----
cx, cy = 0.5, 0.60
ax, ay = 0.40, 0.205
u, v = (x - cx) / ax, (y - cy) / ay
# almond: lens shape from two arcs
almond = np.abs(v) - np.clip(1 - u * u, 0, None) ** 0.9 * np.where(v < 0, 1.0, 0.92)
inside = almond < 0
sock = np.clip(1 - (np.hypot(u * 0.9, v * 0.75) - 0.9) / 0.6, 0, 1)            # shadowy socket
img = mix(img, col('#14210f'), sock * 0.75)

# ---- sclera: sickly yellow-white, darker at the corners, red veins ----
r_e = np.hypot(u, v * 0.8)
sclera = mix(col('#f1ead0'), col('#b9a77a'), smooth(0.35, 1.05, r_e))
vein = np.zeros((S, S), np.float32)
from PIL import ImageDraw
vim = Image.new('L', (S, S), 0); vd = ImageDraw.Draw(vim)
def branch(px, py, ang, length, w):
    for _ in range(int(length)):
        nx, ny = px + math.cos(ang) * 4, py + math.sin(ang) * 4
        vd.line([(px, py), (nx, ny)], fill=255, width=max(1, int(w)))
        px, py = nx, ny; ang += rng.normal(0, 0.18); w *= 0.985
        if rng.random() < 0.04 and w > 1.5: branch(px, py, ang + rng.choice([-0.7, 0.7]), length * 0.5, w * 0.7)
for k in range(9):
    side = -1 if k % 2 else 1
    sx = cx * S + side * ax * S * rng.uniform(0.8, 1.0); sy = cy * S + rng.uniform(-0.12, 0.12) * S
    branch(sx, sy, (math.pi if side > 0 else 0) + rng.normal(0, 0.35), rng.uniform(18, 34), rng.uniform(4, 7))
vein = np.asarray(vim.filter(ImageFilter.GaussianBlur(1.6)), np.float32) / 255
sclera = mix(sclera, col('#b0221b'), vein * 0.85)

# ---- iris: glowing acid green with streaks, slit-ish pupil ----
ix, iy, ir = cx + 0.015, cy + 0.005, 0.175
dx, dy = (x - ix) / ir, (y - iy) / ir
rr, th = np.hypot(dx, dy), np.arctan2(dy, dx)
streak = 0.5 + 0.5 * np.sin(th * 46 + np.sin(th * 7) * 2.0) * np.sin(th * 17 + 1.3)
iris = mix(col('#d8ff6a'), col('#3f8f1c'), smooth(0.25, 0.95, rr))
iris = mix(iris, col('#1e4d10'), streak * smooth(0.35, 1.0, rr) * 0.55)
iris = mix(iris, col('#0f2a08'), smooth(0.86, 1.0, rr))                          # dark limbal ring
pupil = (dx / 0.30) ** 2 + (dy / 0.78) ** 2                                      # vertical slit
iris = mix(iris, col('#050805'), smooth(1.15, 0.95, pupil))
eye = np.where((rr < 1)[..., None], iris, sclera)
# glow of the iris onto the sclera
eye = mix(eye, col('#b6ff5a'), np.clip(1 - (rr - 1) / 0.5, 0, 1) * (rr >= 1) * 0.18)
# shading under the upper lid
eye = mix(eye, col('#3a2a14'), smooth(-0.2, -1.0, v) * 0.55)
img = np.where(inside[..., None], eye, img)

# ---- heavy upper lid (menacing, half-lowered), only inside the eye opening ----
lid_edge = cy - ay * 0.30 + (u * u) * ay * 0.45
lid = inside & (y < lid_edge)
lidcol = mix(col('#5a8a45'), col('#2a4522'), smooth(lid_edge - 0.10, lid_edge, y))   # rounded lid, darker at the rim
img = np.where(lid[..., None], lidcol, img)
img = mix(img, col('#0a1207'), (inside & (np.abs(y - lid_edge) < 0.012)) * 1.0)      # lash line
img = mix(img, col('#000000'), (inside & (y > lid_edge) & (y < lid_edge + 0.05)) * smooth(lid_edge + 0.05, lid_edge, y) * 0.45)  # lid shadow on the eye
img = mix(img, col('#0b1408'), ((np.abs(almond) < 0.03) & (np.abs(u) < 0.985)) * 0.9)  # dark outline
# specular highlight on the eye
hl = ((x - (ix - 0.06)) / 0.035) ** 2 + ((y - (iy - 0.035)) / 0.022) ** 2
img = mix(img, col('#ffffff'), smooth(1.0, 0.3, hl) * (y > lid_edge) * 0.85)
# faint green glow around the eye opening
glow = np.clip(1 - np.clip(almond, 0, None) / 0.5, 0, 1) * (~inside) * (np.abs(u) < 1.1)
img = mix(img, col('#9dff4a'), glow ** 3 * 0.18)

# ---- brow + forehead scar with staples ----
brow_y = 0.36 + (x - 0.5) ** 2 * 0.3                                              # soft brow-ridge shadow
img = mix(img, col('#1c2f18'), smooth(brow_y - 0.08, brow_y + 0.04, y) * smooth(brow_y + 0.16, brow_y + 0.04, y) * 0.45)
scar_y = lambda xx: 0.17 + 0.03 * np.sin(xx * 5.0)
sc = np.abs(y - scar_y(x)) < 0.006
img = np.where(sc[..., None], col('#3b1d18'), img)
for k in range(6):
    sx = 0.12 + k * 0.152; sy = 0.17 + 0.03 * math.sin(sx * 5.0)
    stp = (np.abs(x - sx) < 0.016) & (np.abs(y - sy) < 0.052)
    img = np.where(stp[..., None], mix(col('#c9ccc4'), col('#6d706a'), (y - sy + 0.05) / 0.1), img)

# ---- vignette ----
vig = smooth(0.45, 0.95, np.hypot(x - 0.5, y - 0.52))
img = mix(img, col('#060a05'), vig * 0.6)

out = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).resize((180, 180), Image.LANCZOS)
out = out.filter(ImageFilter.UnsharpMask(radius=1.2, percent=60, threshold=2))
buf = io.BytesIO(); out.save(buf, 'PNG', optimize=True); data = buf.getvalue()
here = os.path.dirname(os.path.abspath(__file__))
if len(sys.argv) > 1: out.save(sys.argv[1])
lines = [',' .join('0x%02X' % b for b in data[i:i + 20]) for i in range(0, len(data), 20)]
src = ('// Home-screen icon (generated by tools/make_icon.py). Served at /apple-touch-icon.png\n'
       '#include <Arduino.h>\nextern const uint8_t ICON_PNG[] PROGMEM = {\n  ' + ',\n  '.join(lines) +
       '\n};\nextern const size_t ICON_PNG_LEN = %d;\n' % len(data))
open(os.path.join(here, '..', 'src', 'icon_png.cpp'), 'w').write(src)
print('icon bytes', len(data))
