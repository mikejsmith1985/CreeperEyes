#!/usr/bin/env python3
"""
Builds src/eyedata/*.h (one per eye style) and src/eyedata/styles.inc (the style table)
from Adafruit's Uncanny Eyes graphics, plus two generated styles (Fish, Bloodshot).

Usage:
  git clone --depth 1 https://github.com/adafruit/Uncanny_Eyes /tmp/ue
  python3 tools/eyegen.py /tmp/ue

Adafruit graphics: Phil Burgess / Paint Your Dragon for Adafruit Industries, MIT license.
"""
import math, os, random, re, sys
from PIL import Image, ImageDraw, ImageFilter

UE = sys.argv[1] if len(sys.argv) > 1 else '/tmp/ue'
GFX = os.path.join(UE, 'uncannyEyes', 'graphics')
CONV = os.path.join(UE, 'convert')
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'src', 'eyedata')
FORK = os.path.join(HERE, 'eyesrc')          # the fork's original human + newt tables
os.makedirs(OUT, exist_ok=True)


def parse_header(path):
    """Return {'sclera': (w,h,list), 'iris':..., 'upper':..., 'lower':..., 'polar':...}."""
    t = open(path).read()
    # keep the asymmetric eyelids (#else branch of SYMMETRICAL_EYELID)
    t = re.sub(r'#ifdef SYMMETRICAL_EYELID.*?#else(.*?)#endif[^\n]*', r'\1', t, flags=re.S)
    defs = {k: int(v) for k, v in re.findall(r'#define\s+(\w+)\s+(\d+)', t)}
    out = {'defs': defs}
    for typ, name, d1, d2, body in re.findall(
            r'const\s+(uint16_t|uint8_t)\s+(\w+)\s*\[([^\]]+)\]\s*\[([^\]]+)\]\s*=\s*\{(.*?)\};', t, flags=re.S):
        key = re.match(r'(sclera|iris|upper|lower|polar)', name).group(1)
        h = int(defs.get(d1.strip(), d1.strip()))
        w = int(defs.get(d2.strip(), d2.strip()))
        vals = [int(x, 16) for x in re.findall(r'0[xX][0-9a-fA-F]+', body)]
        assert len(vals) == w * h, (path, name, len(vals), w, h)
        out[key] = (w, h, vals)
    return out


def to565(rgb):
    r, g, b = rgb[:3]
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def img_table(im):
    im = im.convert('RGB')
    w, h = im.size
    px = im.load()
    return (w, h, [to565(px[x, y]) for y in range(h) for x in range(w)])


def emit_array(f, ctype, name, vals):
    f.write(f'static const {ctype} {name}[{len(vals)}] = {{\n')
    for i in range(0, len(vals), 16):
        f.write('  ' + ','.join('0x%X' % v for v in vals[i:i + 16]) + ',\n')
    f.write('};\n')


# ---------------------------------------------------------------------------------------
# Generated styles
# ---------------------------------------------------------------------------------------
def make_bloodshot():
    """Human sclera with red veins creeping in from the edges, slightly yellowed."""
    random.seed(13)
    im = Image.open(os.path.join(CONV, 'defaultEye', 'sclera.png')).convert('RGB')
    w, h = im.size
    cx, cy = w / 2, h / 2
    # yellow / pink tint on the white of the eye only (bright, low-saturation pixels)
    px = im.load()
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            if r > 150 and g > 150 and b > 140:
                px[x, y] = (min(255, r + 4), max(0, g - 18), max(0, b - 45))
    veins = Image.new('L', (w, h), 0)
    d = ImageDraw.Draw(veins)

    def vein(x, y, ang, length, width):
        for _ in range(length):
            ang += random.uniform(-0.35, 0.35)
            # steer toward the iris
            to = math.atan2(cy - y, cx - x)
            ang += 0.08 * (((to - ang + math.pi) % (2 * math.pi)) - math.pi)
            nx, ny = x + math.cos(ang) * 2, y + math.sin(ang) * 2
            if math.hypot(nx - cx, ny - cy) < 44:      # stop at the iris edge
                return
            d.line([(x, y), (nx, ny)], fill=255, width=max(1, int(round(width))))
            x, y = nx, ny
            width *= 0.985
            if random.random() < 0.06 and width > 0.8:
                vein(x, y, ang + random.choice([-1, 1]) * random.uniform(0.5, 1.1), length // 2, width * 0.7)

    for i in range(26):
        a = i / 26 * 2 * math.pi + random.uniform(-0.1, 0.1)
        r0 = random.uniform(80, 98)
        vein(cx + math.cos(a) * r0, cy + math.sin(a) * r0, a + math.pi, random.randint(14, 28), random.uniform(1.4, 2.6))
    veins = veins.filter(ImageFilter.GaussianBlur(0.6))
    vp = veins.load()
    for y in range(h):
        for x in range(w):
            a = vp[x, y] / 255
            if a > 0.02 and math.hypot(x - cx, y - cy) > 42:
                r, g, b = px[x, y]
                vr, vg, vb = 165, 18, 22
                px[x, y] = (int(r + (vr - r) * a * 0.85), int(g + (vg - g) * a * 0.85), int(b + (vb - b) * a * 0.85))
    return im


def make_fish():
    """Big round black pupil, metallic gold/silver iris filling the whole eye."""
    random.seed(7)
    W, H = 512, 80                       # drawn outer edge first, flipped at the end
    iris = Image.new('RGB', (W, H))
    px = iris.load()
    streak = [random.uniform(-1, 1) for _ in range(W)]
    for i in range(3):                   # smooth the streak noise around the circle
        streak = [(streak[(x - 1) % W] + 2 * streak[x] + streak[(x + 1) % W]) / 4 for x in range(W)]
    speck = [[random.random() for _ in range(W)] for _ in range(H)]
    for y in range(H):
        t = y / (H - 1)
        # t = 0 at the pupil, 1 at the outer edge (after the flip below)
        if t < 0.07:                      # thin dark ring round the pupil
            base = (50, 34, 12)
        elif t < 0.40:                    # rich gold ring
            k = (t - 0.07) / 0.33
            base = (int(245 - 15 * k), int(165 + 10 * k), int(35 + 30 * k))
        elif t < 0.85:                    # gold blending to silver
            k = (t - 0.40) / 0.45
            base = (int(230 - 70 * k), int(175 - 5 * k), int(65 + 115 * k))
        else:                             # dark outer rim
            base = (30, 38, 48)
        for x in range(W):
            s = 1 + 0.18 * streak[x]
            if 0.07 <= t < 0.85 and speck[y][x] < 0.03:
                s *= 0.55                 # dark speckles
            px[x, y] = tuple(max(0, min(255, int(c * s))) for c in base)
    iris = iris.transpose(Image.FLIP_TOP_BOTTOM)   # the firmware's last iris row is next to the pupil
    # sclera: only the pupil (centre) and the far corners ever show -- keep it black/blue-black
    sc = Image.new('RGB', (160, 160), (4, 6, 10))
    return iris, sc


# ---------------------------------------------------------------------------------------
STYLES = [
    # id, label, source, irisMin, irisMax
    ('human',      'Human',            ('fork', 'defaultEye.h'),        150, 400),
    ('newt',       'Newt',             ('fork', 'newtEye.h'),           180, 600),
    ('bloodshot',  'Bloodshot zombie', ('bloodshot',),                  150, 400),
    ('dragon',     'Dragon',           ('ada', 'dragonEye.h'),           80, 400),
    ('goat',       'Goat / Krampus',   ('ada', 'goatEye.h'),            120, 720),
    ('cat',        'Cat',              ('ada', 'catEye.h'),             100, 600),
    ('alien',      'Alien (all iris)', ('ada', 'noScleraEye.h'),        120, 550),
    ('fish',       'Fish',             ('fish',),                       250, 520),
    ('monster',    'Googly monster',   ('ada', 'naugaEye.h'),           300, 1000),
    ('owl',        'Owl',              ('ada', 'owlEye.h'),             300, 800),
    ('terminator', 'Terminator',       ('ada', 'terminatorEye.h'),      120, 720),
]

tables = {}
for sid, label, src, imin, imax in STYLES:
    if src[0] == 'fork':
        tables[sid] = parse_header(os.path.join(FORK, src[1]))
    elif src[0] == 'ada':
        tables[sid] = parse_header(os.path.join(GFX, src[1]))

# Bloodshot = human with a new sclera; Fish = alien geometry with new iris + sclera
tables['bloodshot'] = dict(tables['human'])
tables['bloodshot']['sclera'] = img_table(make_bloodshot())
fi, fs = make_fish()
tables['fish'] = dict(tables['alien'])
tables['fish']['iris'] = img_table(fi)
tables['fish']['sclera'] = img_table(fs)
make_bloodshot().save(os.path.join(HERE, 'eyesrc', 'bloodshot-sclera.png'))
fi.save(os.path.join(HERE, 'eyesrc', 'fish-iris.png'))

# ---------------------------------------------------------------------------------------
# Binary blob for the 'eyes' flash partition (see partitions_creeper.csv, src/eyes.cpp)
#   header: 'CEYE' u16 version u16 count, then count x 64-byte entries, then 4-aligned data
# ---------------------------------------------------------------------------------------
import struct
MAGIC, VERSION, ENTRY = b'CEYE', 1, 64
blobs = bytearray()
offsets = {}                      # id(list) -> offset (shared tables stored once)
data_start = 8 + ENTRY * len(STYLES)

def put(vals, size):
    key = id(vals)
    if key not in offsets:
        while (data_start + len(blobs)) % 4: blobs.append(0)
        offsets[key] = data_start + len(blobs)
        blobs.extend(struct.pack('<%d%s' % (len(vals), 'H' if size == 2 else 'B'), *vals))
    return offsets[key]

entries = bytearray()
for sid, label, src, imin, imax in STYLES:
    t = tables[sid]
    sw, sh, sv = t['sclera']; iw, ih, iv = t['iris']; pw, ph, pv = t['polar']
    assert t['upper'][0] == 128 and t['lower'][0] == 128
    e = struct.pack('<24sIHHIHHIIIHHhh', label.encode()[:23],
                    put(sv, 2), sw, sh, put(iv, 2), iw, ih,
                    put(t['upper'][2], 1), put(t['lower'][2], 1), put(pv, 2), pw, ph, imin, imax)
    entries.extend(e.ljust(ENTRY, b'\0'))
blob = MAGIC + struct.pack('<HH', VERSION, len(STYLES)) + entries + blobs
outbin = os.path.join(HERE, '..', 'eyedata', 'eyes.bin')
os.makedirs(os.path.dirname(outbin), exist_ok=True)
open(outbin, 'wb').write(blob)
print('eyes.bin bytes:', len(blob))
