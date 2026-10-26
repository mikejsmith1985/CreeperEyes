# Bust plinth whose pocket is cut from the bust's own bottom (wall shape), offset 0.5 mm.
import numpy as np, trimesh, manifold3d as mf
from scipy import ndimage
from skimage import measure
from shapely.geometry import Polygon
from shapely.ops import unary_union
OFF, DEPTH, PH, R = 0.5, 4.0, 12.0, 0.1
import time; T0 = time.time()
b = trimesh.load('/mnt/user-data/uploads/frankenstein_stls/CreeperEyes_NeckShoulders_paintready_nostitches.stl')
b = b.slice_plane([0, 0, 14.0], [0, 0, -1.0], cap=True)            # only the bottom 14 mm matters
print('cropped', len(b.faces), round(time.time() - T0), 's', flush=True)
B = mf.Manifold(mf.Mesh(vert_properties=b.vertices.astype(np.float32), tri_verts=b.faces.astype(np.uint32)))
lo = np.array([-100.0, -100.0]); n = int(200 / R)
xs = lo[0] + (np.arange(n) + 0.5) * R; ys = lo[1] + (np.arange(n) + 0.5) * R
from PIL import Image, ImageDraw
occ = np.zeros((n, n), bool)
for z in np.arange(0.05, DEPTH + 0.6, 0.25):          # everything the bust has in the pocket depth (+0.5)
    acc = np.zeros((n, n), bool)
    for p in B.slice(z).to_polygons():
        if len(p) < 3: continue
        im = Image.new('1', (n, n), 0)
        ImageDraw.Draw(im).polygon([((x - lo[0]) / R, (y - lo[1]) / R) for x, y in p], fill=1)
        acc ^= np.asarray(im, bool).T                   # even-odd: holes cancel; index [x, y]
    occ |= acc
print('bust wall area in pocket (mm2)', occ.sum() * R * R, round(time.time() - T0), 's', flush=True)
dist = ndimage.distance_transform_edt(~occ, sampling=R)
cut = (dist <= OFF)                                    # wall shape grown by 0.5 mm all round
cs = measure.find_contours(np.pad(cut, 1).astype(np.float32), 0.5)
polys = []
for c in cs:
    xy = (c - 1) * R + lo + R / 2
    if len(xy) > 8: polys.append(xy[:, ::1])
rings = [Polygon(p).buffer(0) for p in polys]
outer = max(rings, key=lambda p: p.area)
groove = unary_union([r for r in rings if r.area > 50])
# even-odd of all contours = the groove (outer outline minus the bust's interior)
cs2d = mf.CrossSection([np.asarray(p) for p in polys], mf.FillRule.EvenOdd)
print('groove area', round(cs2d.area(), 1), 'contours', len(polys))
outline = Polygon(outer.exterior)
plate = outline.buffer(10).buffer(-4).buffer(4)
x0, y0, x1, y1 = plate.bounds
def CS(g):
    gs = list(g.geoms) if hasattr(g, 'geoms') else [g]; r = []
    for p in gs: r.append(np.array(p.exterior.coords[:-1])); r += [np.array(i.coords[:-1]) for i in p.interiors]
    return mf.CrossSection(r, mf.FillRule.EvenOdd)
pl = CS(plate).extrude(PH)
pl = pl - cs2d.extrude(DEPTH + 0.1).translate([0, 0, PH - DEPTH])
# hollow the middle (well inside the bust's wall) to save plastic; keeps a solid ledge under the groove
inn = outline.convex_hull.buffer(-12)
pl = pl - CS(inn).extrude(PH).translate([0, 0, 3.0])
pl = pl.translate([-x0, -y0, 0])
bx1, by1 = x1 - x0, y1 - y0
pl = pl - mf.Manifold.cube([14, 40, 7]).translate([bx1 / 2 - 7, by1 - 30, PH - 7])      # cable out the back
pl = pl - mf.Manifold.cube([40, 14, 7]).translate([bx1 - 30, by1 / 2 - 7, PH - 7])      # power out the side
m = pl.to_mesh(); t = trimesh.Trimesh(m.vert_properties[:, :3], m.tri_verts)
print('plinth', (t.bounds[1] - t.bounds[0]).round(1), 'vol cm3', round(pl.volume() / 1000), 'pieces', len(pl.decompose()), 'status', pl.status())
t.export('stl/CreeperBackdrop_6_bust_plinth.stl')
# fit check: bust bottom (moved onto the plinth) must not intersect it
bust_on = B.translate([-x0, -y0, PH - DEPTH])
print('overlap bust vs plinth mm3', round((bust_on ^ pl).volume(), 3))
bb = bust_on.trim_by_plane([0, 0, -1], -(PH + 8)); bm = bb.to_mesh()
trimesh.Trimesh(bm.vert_properties[:, :3], bm.tri_verts).export('fit_bust_bottom.stl')
np.save('plinth_xy0.npy', np.array([x0, y0]))
