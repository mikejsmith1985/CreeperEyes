# Builds every printable part of the CreeperEyes stormy-graveyard backdrop (Prusa XL, 360 x 360 bed).
import sys, numpy as np, trimesh, manifold3d as mf
from shapely.geometry import box, Polygon
from shapely import affinity
sys.path.insert(0, '.')
import scene

S = 314.0 / scene.W            # art drawn at 220, built at 314 (330 mm parts leave room on the XL bed)
V = 314.0                      # visible window
B = 8.0                        # border clamped between frames
P = V + 2 * B                  # 330: layer / ring / light box size
LAYER_T, GAP = 2.0, 10.0       # silhouette thickness, depth between layers
PIN_D, PIN_IN = 2.2, 4.0       # alignment pin holes (use 1.75 mm filament as pins)
LITHO_MIN, LITHO_MAX = 0.6, 3.2
BOX_D, WALL = 36.0, 2.4        # light box depth (LEDs ~30 mm behind the lithophane), wall
out = {}

def cs_from_shapely(g):
    polys = list(g.geoms) if hasattr(g, 'geoms') else [g]
    rings = []
    for p in polys:
        if p.is_empty: continue
        rings.append(np.array(p.exterior.coords[:-1]))
        rings += [np.array(i.coords[:-1]) for i in p.interiors]
    return mf.CrossSection(rings, mf.FillRule.EvenOdd)

def pins(z0, h):
    m = mf.Manifold()
    for x in (PIN_IN, P - PIN_IN):
        for y in (PIN_IN, P - PIN_IN):
            m = m + mf.Manifold.cylinder(h + 2, PIN_D / 2, PIN_D / 2, 24).translate([x, y, z0 - 1])
    return m

def to_tm(m):
    mm = m.to_mesh(); return trimesh.Trimesh(mm.vert_properties[:, :3], mm.tri_verts)

def art(g):  # scene geometry -> plate coordinates (scaled, shifted into the border)
    return affinity.translate(affinity.scale(g, S, S, origin=(0, 0)), B, B)

border = box(0, 0, P, P).difference(box(B, B, P - B, P - B))
def layer(g, name):
    shape = art(g).intersection(box(B - 1, B - 1, P - B + 1, P - B + 1)).union(border)
    m = cs_from_shapely(shape).extrude(LAYER_T) - pins(0, LAYER_T)
    out[name] = m

hill, windows = scene.hill_layer()
layer(hill, '2_house_and_hill')
layer(scene.tree_layer(), '3_dead_tree')
layer(scene.fore_layer(), '4_graves_and_fence')

# ---------------- lithophane sky (back layer). Flat face = front (print it face down); relief faces the LEDs.
t, res = scene.sky_relief(res=0.4)
t = np.clip(t, LITHO_MIN, LITHO_MAX)[::-1]                 # row 0 = bottom (y up)
pitch = res * S
nb = int(round(B / pitch))
T = np.full((t.shape[0] + 2 * nb, t.shape[1] + 2 * nb), LITHO_MAX, np.float32)
T[nb:nb + t.shape[0], nb:nb + t.shape[1]] = t
ny, nx = T.shape
xs = np.arange(nx) * pitch * (P / ((nx - 1) * pitch)); ys = np.arange(ny) * pitch * (P / ((ny - 1) * pitch))
X, Y = np.meshgrid(xs, ys)
top = np.c_[X.ravel(), Y.ravel(), T.ravel()]
bot = np.c_[X.ravel(), Y.ravel(), np.zeros(X.size)]
idx = np.arange(nx * ny).reshape(ny, nx)
a, b, c, d = idx[:-1, :-1].ravel(), idx[:-1, 1:].ravel(), idx[1:, 1:].ravel(), idx[1:, :-1].ravel()
F = [np.c_[a, b, c], np.c_[a, c, d]]                       # top (normals up)
o = nx * ny
F += [np.c_[a + o, c + o, b + o], np.c_[a + o, d + o, c + o]]  # bottom
def wall(edge):                                            # edge: ordered vertex ids along the boundary (ccw)
    e0, e1 = edge[:-1], edge[1:]
    return [np.c_[e0, e0 + o, e1], np.c_[e1, e0 + o, e1 + o]]
ring = np.r_[idx[0, :], idx[1:, -1], idx[-1, -2::-1], idx[-2:0:-1, 0], idx[0, 0]]
F += wall(ring)
lit = trimesh.Trimesh(np.r_[top, bot], np.vstack(F), process=True)
if lit.volume < 0: lit.invert()
print('lithophane watertight', lit.is_watertight, 'tris', len(lit.faces))
L = mf.Manifold(mf.Mesh(vert_properties=lit.vertices.astype(np.float32), tri_verts=lit.faces.astype(np.uint32)))
out['1_sky_lithophane'] = L - pins(0, LITHO_MAX)

# ---------------- spacer rings (x3) and front bezel
ring2d = mf.CrossSection.square([P, P]) - mf.CrossSection.square([V, V]).translate([B, B])
out['spacer_ring_x3'] = ring2d.extrude(GAP) - pins(0, GAP)
BZ = 0.0                                                   # bezel is the same size as the stack (356 mm fits the XL bed)
bez = mf.CrossSection.square([P, P]).extrude(4.0)
cham = mf.Manifold.hull(mf.CrossSection.square([V - 6, V - 6]).translate([B + 3, B + 3]).extrude(0.01).translate([0, 0, -0.01])
                        + mf.CrossSection.square([V, V]).translate([B, B]).extrude(0.01).translate([0, 0, 3.0]))
cham = cham + mf.CrossSection.square([V, V]).translate([B, B]).extrude(1.1).translate([0, 0, 2.95])
out['5_front_bezel'] = bez - cham - pins(0, 4.0)          # front face is z=0; inner edge chamfered 45 deg (print front face up)

# ---------------- LED light box (open front, white inside): LED rows on the back wall
box_out = mf.CrossSection.square([P, P]).extrude(BOX_D)
BACK = 1.6
box_in = mf.CrossSection.square([P - 2 * WALL, P - 2 * WALL]).translate([WALL, WALL]).extrude(BOX_D).translate([0, 0, BACK])
lb = box_out - box_in
# front flange keeps the pin pattern: solid border of width B at the open end
flange = (mf.CrossSection.square([P, P]) - mf.CrossSection.square([V, V]).translate([B, B])).extrude(2.0).translate([0, 0, BOX_D - 2.0])
lb = lb + flange - pins(BOX_D - 2.0, 2.0)
# wire exit (bottom edge, centre) + 3 shallow guide ridges for the strip rows on the back wall
lb = lb - mf.Manifold.cube([16, WALL + 2, 9]).translate([P / 2 - 8, -1, BACK + 2])
for r in range(3):
    yr = B + V * (r + 0.5) / 3
    for dy in (-6.5, 6.5):                                 # 12 mm channel between two 1.2 mm ridges
        lb = lb + mf.Manifold.cube([V - 10, 1.2, 1.5]).translate([B + 5, yr + dy - 0.6, BACK])
out['0_led_light_box'] = lb

# ---------------- bust plinth (the bust drops into a 4 mm pocket) + two feet that hold the backdrop upright
bust = trimesh.load('/mnt/user-data/uploads/frankenstein_stls/CreeperEyes_NeckShoulders_paintready_nostitches.stl')
from shapely.geometry import MultiPoint
pts = np.vstack([np.asarray(p)[:, :2] for z in (1.0, 2.0, 4.0) for p in bust.section([0, 0, 1], [0, 0, z]).discrete])
foot = MultiPoint(pts).convex_hull.buffer(0.8)             # bust footprint + 0.8 mm clearance
PH = 12.0
pl = foot.buffer(10).buffer(-4).buffer(4)
x0, y0, _, _ = pl.bounds
pl = affinity.translate(pl, -x0, -y0); fb = affinity.translate(foot, -x0, -y0)
plm = cs_from_shapely(pl).extrude(PH)
plm = plm - cs_from_shapely(fb).extrude(4.1).translate([0, 0, PH - 4.0])          # pocket
plm = plm - cs_from_shapely(fb.buffer(-8)).extrude(PH).translate([0, 0, 3.0])     # hollow under the bust
bx0, by0, bx1, by1 = pl.bounds
plm = plm - mf.Manifold.cube([14, 40, 7]).translate([(bx0 + bx1) / 2 - 7, by1 - 30, PH - 7])   # cable out the back
plm = plm - mf.Manifold.cube([40, 14, 7]).translate([bx1 - 30, (by0 + by1) / 2 - 7, PH - 7])   # power cable out the side
out['6_bust_plinth'] = plm
STACK = BOX_D + LITHO_MAX + 3 * (GAP + LAYER_T) + 4.0      # light box + layers + bezel depth
fw, fl, fh, fwl = STACK + 2.0, 70.0, 22.0, 4.0              # slot width (+1 mm a side: 9 stacked prints add up), foot length, height, wall
ft = mf.Manifold.cube([fl, fw + 2 * fwl + 70, 3.0])         # wide flat sole for stability
ft = ft + mf.Manifold.cube([fl, fwl, fh]).translate([0, 35, 0]) + mf.Manifold.cube([fl, fwl, fh]).translate([0, 35 + fwl + fw, 0])
out['7_backdrop_foot_x2'] = ft
print('plinth', (np.array(pl.bounds[2:]) - pl.bounds[:2]).round(0), 'stack depth', round(STACK, 1))

import os
os.makedirs('stl', exist_ok=True)
for k, m in out.items():
    tm = to_tm(m)
    if tm.is_empty: print(k, 'EMPTY'); continue
    tm.apply_translation(-tm.bounds[0])
    tm.export(f'stl/CreeperBackdrop_{k}.stl')
    ext = tm.bounds[1] - tm.bounds[0]
    print(f'{k:22s} {ext.round(1)} mm  watertight={tm.is_watertight}  pieces={len(m.decompose())}  {tm.volume/1000:.0f} cm3')
