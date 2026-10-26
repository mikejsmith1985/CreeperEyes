# Original layered "stormy graveyard" shadow-box scene for the CreeperEyes backdrop.
# All shapes in mm, visible window 220 x 220, origin bottom-left. Builds 2D layers with shapely.
import math, random
import numpy as np
from shapely.geometry import Polygon, Point, LineString, box, MultiPolygon
from shapely.ops import unary_union
from shapely import affinity

W = H = 220.0
CROW = (74, 88)
BATS = ((62, 168, 1.0), (82, 182, 0.7), (118, 192, 0.55))
rnd = random.Random(13)

def bez(p0, p1, p2, n=24):
    t = np.linspace(0, 1, n)[:, None]
    return ((1 - t) ** 2 * np.array(p0) + 2 * (1 - t) * t * np.array(p1) + t ** 2 * np.array(p2)).tolist()

def arch_window(cx, by, w, h):
    """Gothic pointed-arch window, bottom centre (cx, by)."""
    sh = h - w * 0.75
    pts = [(cx - w / 2, by), (cx + w / 2, by), (cx + w / 2, by + sh)]
    pts += bez((cx + w / 2, by + sh), (cx + w / 2, by + h * 0.92), (cx, by + h))[1:]
    pts += bez((cx, by + h), (cx - w / 2, by + h * 0.92), (cx - w / 2, by + sh))[1:]
    win = Polygon(pts)
    mull = unary_union([box(cx - 0.6, by, cx + 0.6, by + h), box(cx - w / 2, by + sh * 0.55 - 0.6, cx + w / 2, by + sh * 0.55 + 0.6)])
    return win.difference(mull)

def witch_roof(x0, x1, base_y, peak_h, curl):
    """Sagging roof with a curled-over tip."""
    mx = (x0 + x1) / 2
    L = bez((x0 - 4, base_y), (mx - 2, base_y + peak_h * 0.35), (mx + curl * 0.3, base_y + peak_h))
    R = bez((mx + curl * 0.3, base_y + peak_h), (mx + 4, base_y + peak_h * 0.35), (x1 + 4, base_y))
    tip = bez((mx + curl * 0.3, base_y + peak_h), (mx + curl, base_y + peak_h + 6), (mx + curl * 1.3, base_y + peak_h - 3))
    poly = Polygon(L + R[1:] + [(x1 + 4, base_y - 2.5), (x0 - 4, base_y - 2.5)])
    return unary_union([poly, LineString(tip).buffer(1.6, cap_style=1)])

# ------------------------------------------------------------------ far hill + crooked house
def hill_layer():
    xs = np.linspace(0, W, 60)
    ys = 58 + 18 * np.sin((xs - 40) / W * math.pi) + 4 * np.sin(xs / 17)
    hill = Polygon([(0, 0)] + list(zip(xs, ys)) + [(W, 0)])
    hx, hy = 150, 70                                   # house base on the hill
    body = Polygon([(hx - 26, hy), (hx + 22, hy), (hx + 25, hy + 46), (hx - 22, hy + 50)])
    upper = Polygon([(hx - 16, hy + 46), (hx + 18, hy + 44), (hx + 21, hy + 74), (hx - 12, hy + 78)])
    tower = Polygon([(hx - 40, hy), (hx - 24, hy), (hx - 23, hy + 82), (hx - 37, hy + 84)])
    roof1 = witch_roof(hx - 26, hx + 25, hy + 47, 16, 6)
    roof2 = witch_roof(hx - 15, hx + 21, hy + 75, 22, -7)
    roof3 = witch_roof(hx - 40, hx - 22, hy + 82, 34, 9)
    chimney = Polygon([(hx + 10, hy + 80), (hx + 16, hy + 79), (hx + 17, hy + 94), (hx + 11, hy + 95)])
    porch = witch_roof(hx - 6, hx + 14, hy + 20, 8, 3)
    house = unary_union([body, upper, tower, roof1, roof2, roof3, chimney, porch])
    house = affinity.rotate(house, 2.5, origin=(hx, hy))           # the whole thing leans
    windows = unary_union([
        arch_window(hx - 12, hy + 26, 8, 13), arch_window(hx + 12, hy + 24, 8, 13),
        arch_window(hx - 2, hy + 54, 9, 14), arch_window(hx - 30.5, hy + 52, 6, 11),
        arch_window(hx - 30.5, hy + 22, 6, 11), box(hx + 0, hy, hx + 8, hy + 14)])
    windows = affinity.rotate(windows, 2.5, origin=(hx, hy))
    # bat-like crows/bats
    bats = []
    for bx, by, s in ((62, 168, 1.0), (82, 182, 0.7), (118, 192, 0.55)):
        wing = Polygon([(0, 0), (-4, 2), (-7, 1), (-9, 3), (-11, 0.5), (-8, -1), (-4, -1.2)])
        b = unary_union([wing, affinity.scale(wing, -1, 1, origin=(0, 0)), Point(0, 0.3).buffer(1.3)])
        bats.append(affinity.translate(affinity.scale(b, s, s, origin=(0, 0)), bx, by))
    return unary_union([hill, house]).difference(windows), windows

# ------------------------------------------------------------------ dead tree
def tree_layer():
    segs = []
    def grow(x, y, ang, length, w, depth):
        if depth == 0 or w < 0.7: return
        pts = [(x, y)]
        a = ang
        for _ in range(6):
            a += rnd.uniform(-0.22, 0.22)
            x += math.cos(a) * length / 6; y += math.sin(a) * length / 6
            pts.append((x, y))
        segs.append(LineString(pts).buffer(w, cap_style=1, join_style=1))
        n = 2 if depth > 2 else rnd.choice([1, 2, 2, 3])
        for k in range(n):
            na = a + rnd.uniform(0.35, 0.8) * (1 if k % 2 else -1) + rnd.uniform(-0.15, 0.15)
            grow(x, y, na, length * rnd.uniform(0.62, 0.8), w * 0.62, depth - 1)
    # trunk: gnarled, leaning right
    trunk = Polygon([(18, 0), (52, 0), (44, 18), (40, 60), (44, 100), (36, 104), (30, 60), (24, 20)])
    segs.append(trunk)
    grow(37, 100, math.radians(78), 46, 4.2, 6)       # crown
    grow(40, 70, math.radians(28), 30, 3.0, 4)        # branch reaching towards the house
    grow(31, 82, math.radians(150), 30, 2.6, 4)       # branch to the left edge
    tree = unary_union(segs)
    # crow on the long branch
    crow = unary_union([Point(0, 0).buffer(4.2).intersection(box(-5, -5, 5, 5)),
                        Point(4.5, 3.2).buffer(2.4),
                        Polygon([(6.5, 3.6), (10, 3.0), (6.5, 2.4)]),
                        Polygon([(-3.5, -1), (-10, -4), (-3, -3.5)])])
    crow = affinity.scale(crow, 1.0, 0.85)
    tree = unary_union([tree, affinity.translate(crow, CROW[0], CROW[1])])
    ground = Polygon([(0, 0), (0, 14)] + [(x, 14 + 3 * math.sin(x / 13)) for x in np.linspace(0, 80, 20)] + [(80, 0)])
    return unary_union([tree, ground]).intersection(box(0, 0, W, H))

# ------------------------------------------------------------------ foreground graves + fence
def tombstone(cx, by, w, h, kind, lean):
    if kind == 'round':
        s = unary_union([box(cx - w / 2, by, cx + w / 2, by + h - w / 2), Point(cx, by + h - w / 2).buffer(w / 2)])
    elif kind == 'cross':
        s = unary_union([box(cx - w * 0.16, by, cx + w * 0.16, by + h), box(cx - w / 2, by + h * 0.62, cx + w / 2, by + h * 0.62 + w * 0.32)])
    else:  # peaked
        s = Polygon([(cx - w / 2, by), (cx + w / 2, by), (cx + w / 2, by + h * 0.8), (cx, by + h), (cx - w / 2, by + h * 0.8)])
    s = affinity.rotate(s, lean, origin=(cx, by))
    s = unary_union([s, box(cx - w * 0.3, 0, cx + w * 0.3, by + 3)])   # buried foot: always joined to the ground
    if kind != 'cross':  # carved cross / RIP crack
        crack = LineString([(cx - w * 0.2, by + h * 0.75), (cx, by + h * 0.55), (cx - w * 0.1, by + h * 0.4), (cx + w * 0.15, by + h * 0.25)]).buffer(0.55)
        s = s.difference(affinity.rotate(crack, lean, origin=(cx, by)))
    return s

def fore_layer():
    xs = np.linspace(0, W, 80)
    ys = 16 + 5 * np.sin(xs / 23 + 1) + 2 * np.sin(xs / 7)
    ground = Polygon([(0, 0)] + list(zip(xs, ys)) + [(W, 0)])
    stones = [tombstone(92, 14, 16, 26, 'round', -6), tombstone(118, 16, 11, 30, 'cross', 7),
              tombstone(70, 12, 12, 19, 'peaked', 4), tombstone(140, 15, 13, 20, 'round', -3)]
    # wrought iron fence on the right, one picket bent
    fence = [box(158, 30, W, 32.2), box(158, 46, W, 48.2)]
    for i, x in enumerate(np.arange(162, W, 8.5)):
        top = 56 + (3 if i % 2 else 0)
        p = unary_union([box(x - 1.1, 14, x + 1.1, top), Polygon([(x - 2.6, top), (x + 2.6, top), (x, top + 6)])])
        if i == 3: p = affinity.rotate(p, -11, origin=(x, 14))
        fence.append(p)
    fence.append(Point(160, 31).buffer(2.4))
    return unary_union([ground] + stones + fence).intersection(box(0, 0, W, H))

# ------------------------------------------------------------------ back lithophane: sky
def sky_relief(res=0.25):
    """Thickness map (mm) for the lithophane: thin = bright. Moon, swirling storm clouds, rain streaks."""
    n = int(W / res)
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32) * res
    yy = H - yy                                           # row 0 = top
    rng = np.random.default_rng(4)
    t = np.full((n, n), 1.5, np.float32)                  # night sky: medium
    # moon (thin = bright) with a darker halo ring, top-left of the house
    md = np.hypot(xx - 168, yy - 190)
    t = np.where(md < 17, 0.6, t)
    t = np.where((md >= 17) & (md < 22), 1.1 + (md - 17) / 5 * 0.4, t)
    craters = [(163, 194, 3.5), (172, 185, 2.5), (166, 183, 1.8), (175, 196, 2.0)]
    for cx, cy, r in craters:
        t = np.where(np.hypot(xx - cx, yy - cy) < r, 0.85, t)
    # storm clouds: banded turbulence, thick (dark) with thin bright edges where lightning glows
    def fbm(scale, octaves=5):
        acc = np.zeros((n, n), np.float32); amp = 1.0
        from PIL import Image
        for o in range(octaves):
            g = rng.random((int(n / scale) + 2,) * 2).astype(np.float32)
            im = Image.fromarray((g * 255).astype(np.uint8)).resize((n, n), Image.BICUBIC)
            acc += amp * (np.asarray(im, np.float32) / 255 - 0.5); amp *= 0.5; scale /= 2
        return acc
    f = fbm(110)
    band = np.exp(-((yy - 150 - 18 * np.sin(xx / 35)) / 30) ** 2) + 0.7 * np.exp(-((yy - 205 - 8 * np.sin(xx / 20 + 2)) / 14) ** 2)
    cloud = np.clip(band * 1.3 + f * 1.6 - 0.35, 0, 1)
    cloud = np.where(md < 24, cloud * np.clip((md - 18) / 10, 0, 1), cloud)    # clouds part around the moon
    t = t + cloud * 1.6
    edge = np.clip(1 - np.abs(cloud - 0.15) / 0.08, 0, 1)                      # silver linings
    t = t - edge * 0.5
    # lightning bolt across the sky (thinnest = brightest flash)
    bolt = [(58, 220), (66, 196), (58, 190), (72, 160), (64, 156), (80, 122)]
    from PIL import Image, ImageDraw
    im = Image.new('L', (n, n), 0); d = ImageDraw.Draw(im)
    P = [((x / res), ((H - y) / res)) for x, y in bolt]
    d.line(P, fill=255, width=int(1.6 / res), joint='curve')
    d.line([P[3], ((90) / res, (H - 150) / res)], fill=255, width=int(0.9 / res))
    bm = np.asarray(im, np.float32) / 255
    t = np.where(bm > 0.5, 0.5, t)
    bim = Image.new('L', (n, n), 0); bd = ImageDraw.Draw(bim)
    for bx, by, s in BATS:
        wing = [(0, 0), (-4, 2), (-7, 1), (-9, 3), (-11, 0.5), (-8, -1), (-4, -1.2)]
        for sg in (1, -1):
            bd.polygon([((bx + sg * x * s) / res, (H - (by + y * s)) / res) for x, y in wing], fill=255)
        r = 1.3 * s / res; bd.ellipse([bx / res - r, (H - by) / res - r, bx / res + r, (H - by) / res + r], fill=255)
    t = np.where(np.asarray(bim) > 128, 3.2, t)
    # lower half (behind the hills) doesn't matter much; keep it medium-thin for an even glow
    return np.clip(t, 0.5, 3.2), res
