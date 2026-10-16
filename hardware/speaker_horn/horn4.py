import trimesh, numpy as np, manifold3d as mf, itertools, time
from scipy import ndimage
from skimage import measure
g = np.load('geo.npz'); n, ci = g['n'], g['ci']
reg = trimesh.load('region2.stl')

# ---------- local frame at the grill ----------
w = -n / np.linalg.norm(n); u = np.cross(w, [0, 0, 1.0]); u /= np.linalg.norm(u); v = np.cross(u, w); v /= np.linalg.norm(v)
if v[2] < 0: v, u = -v, -u
def on_plane(p): t = -np.dot(n, p - ci) / n[1]; return p + np.array([0, t, 0])
O = on_plane(np.array([62.3, 0, 17.2]))
cl = np.array([[np.dot(on_plane(np.array([x, 0, z])) - O, u), np.dot(on_plane(np.array([x, 0, z])) - O, v)] for x in (47.0, 77.6) for z in (8.0, 26.4)])
gu0, gu1, gv0, gv1 = cl[:, 0].min(), cl[:, 0].max(), cl[:, 1].min(), cl[:, 1].max()
M = np.eye(4); M[:3, 0], M[:3, 1], M[:3, 2], M[:3, 3] = u, v, w, O
Minv = np.linalg.inv(M)

# ---------- voxel solid of the bust shell (local coords) ----------
loc = reg.copy(); loc.apply_transform(Minv)
P = 0.35
lo = np.array([-60, -45, -12.0]); hi = np.array([60, 75, 60.0])
keep = np.all((loc.triangles_center > lo - 2) & (loc.triangles_center < hi + 2), axis=1)
loc = loc.submesh([np.nonzero(keep)[0]], append=True)
shape = np.ceil((hi - lo) / P).astype(int) + 1
occ = np.zeros(shape, bool)
pts, _ = trimesh.sample.sample_surface_even(loc, 2_500_000, radius=P * 0.5) if False else (None, None)
# dense surface sampling (fast + memory-light): barycentric points on every triangle
tri = loc.triangles
k = 6
for a in range(k + 1):
    for b in range(k + 1 - a):
        c_ = k - a - b
        q = (tri[:, 0] * a + tri[:, 1] * b + tri[:, 2] * c_) / k
        idx = np.floor((q - lo) / P).astype(int)
        ok = np.all((idx >= 0) & (idx < shape), axis=1)
        occ[idx[ok, 0], idx[ok, 1], idx[ok, 2]] = True
# close the 3 mm shell between its two faces (and the grill holes), then fill enclosed pockets
occ = ndimage.binary_closing(occ, structure=ndimage.generate_binary_structure(3, 1), iterations=6)
occ = ndimage.binary_fill_holes(occ)
print('shell voxels', occ.sum(), 'grid', shape)
np.save('occ.npy', occ)

def occupied(Lp):
    idx = np.floor((Lp - lo) / P).astype(int)
    ok = np.all((idx >= 0) & (idx < shape), axis=1)
    out = np.zeros(len(Lp), bool); out[ok] = occ[idx[ok, 0], idx[ok, 1], idx[ok, 2]]
    return out

# ---------- part parameters ----------
SPK_D, CLR, WALL, POCKET, LIP, THROAT_R, FL = 40.0, 0.35, 2.2, 7.0, 1.6, 17.5, 6.0
R_out = SPK_D/2 + CLR + WALL
mouth_in  = (gu0 - 1.2, gu1 + 1.2, gv0 - 1.2, gv1 + 1.2)
mouth_out = (mouth_in[0] - WALL, mouth_in[1] + WALL, mouth_in[2] - WALL, mouth_in[3] + WALL)
flange    = (mouth_out[0] - FL + WALL, mouth_out[1] + FL - WALL, mouth_out[2] - FL + WALL, mouth_out[3] + FL - WALL)
Z0 = -6.0                                     # build from outside the wall; the bust solid trims it to fit
cu, cv = (gu0 + gu1)/2, (gv0 + gv1)/2

def rot(p, yaw, pitch=0):
    a = np.radians(yaw); c, s = np.cos(a), np.sin(a)
    p = np.stack([p[..., 0]*c + p[..., 2]*s, p[..., 1], -p[..., 0]*s + p[..., 2]*c], -1)
    b = np.radians(pitch); c, s = np.cos(b), np.sin(b)
    return np.stack([p[..., 0], p[..., 1]*c - p[..., 2]*s, p[..., 1]*s + p[..., 2]*c], -1)
rng = np.random.default_rng(1)
# sample points filling the speaker ring+pocket envelope (in speaker frame) and the horn envelope
rr = np.sqrt(rng.random(6000)) * R_out; aa = rng.random(6000) * 2*np.pi; zz = rng.random(6000) * (LIP + POCKET)
RING = np.stack([rr*np.cos(aa), rr*np.sin(aa), zz], 1)
def world_z(Lp): return (Lp @ np.stack([u, v, w])) [:, 2] + O[2] if False else (O[None] + Lp[:, :1]*u + Lp[:, 1:2]*v + Lp[:, 2:3]*w)[:, 2]
def score(su, sv, yaw, pitch, horn):
    c = np.array([su, sv, horn])
    ring = rot(RING, yaw, pitch) + c
    if world_z(ring).min() < 1.5: return None          # must clear the table
    col = occupied(ring).sum()
    # horn interior: sample segment between mouth points and throat disk (channel must be free beyond t>2)
    m = np.stack([rng.uniform(mouth_in[0], mouth_in[1], 3000), rng.uniform(mouth_in[2], mouth_in[3], 3000), np.zeros(3000)], 1)
    ra = np.sqrt(rng.random(3000)) * THROAT_R; ta = rng.random(3000) * 2*np.pi
    th = rot(np.stack([ra*np.cos(ta), ra*np.sin(ta), np.zeros(3000)], 1), yaw, pitch) + c
    f = rng.random((3000, 1)); hp = m * (1 - f) + th * f
    hp = hp[hp[:, 2] > 3.0]
    col += occupied(hp).sum()
    return col
best = None; t0 = time.time()
for horn, yaw, pitch, du, dv in itertools.product((12, 16, 20, 24), (-20, -10, 0, 10), (-15, 0, 15), range(-14, 7, 2), range(0, 25, 2)):
    sc_ = score(cu + du, cv + dv, yaw, pitch, horn)
    if sc_ is None: continue
    tot = sc_ * 100 + abs(du) * 0.5 + abs(dv) * 0.5 + abs(yaw) * 0.1 + abs(pitch) * 0.1 + horn * 0.3
    if best is None or tot < best[0]: best = (tot, sc_, horn, yaw, pitch, du, dv)
print('BEST', best, round(time.time() - t0), 's')
np.savez('best.npz', best=np.array(best[1:]), O=O, u=u, v=v, w=w, lo=lo, P=P,
         geo=np.array([gu0, gu1, gv0, gv1]))
