import trimesh, numpy as np
m = trimesh.load('region.stl')
def hits(p, d):
    l, _, _ = m.ray.intersects_location([p], [d], multiple_hits=True)
    return sorted(np.dot(l - p, d)) if len(l) else []
# outer + inner surface samples just around the grill (between holes is unreliable; use a ring 2-6 mm outside it)
pts_o, pts_i = [], []
for x in np.arange(44, 81, 1.0):
    for z in np.arange(6, 29, 1.0):
        inside = 47 <= x <= 77.5 and 8.2 <= z <= 26.2
        if inside: continue
        h = hits(np.array([x, -200.0, z]), np.array([0, 1.0, 0]))
        if len(h) >= 2:
            pts_o.append([x, -200 + h[0], z]); pts_i.append([x, -200 + h[1], z])
po, pi = np.array(pts_o), np.array(pts_i)
co = po.mean(0); u, s, vt = np.linalg.svd(po - co); n = vt[2]
if n[1] > 0: n = -n
th = np.linalg.norm(pi - po, axis=1)
print('outer centre', co.round(2), 'normal (outward)', n.round(3), 'wall thickness median %.2f min %.2f max %.2f' % (np.median(th), th.min(), th.max()))
ci = pi.mean(0); print('inner centre', ci.round(2))
gc = np.array([62.2, 0, 17.2]); gc[1] = co[1] + (-(n[0]*(gc[0]-co[0]) + n[2]*(gc[2]-co[2])) / n[1])
print('grill centre on outer plane', gc.round(2))
np.savez('geo.npz', co=co, n=n, ci=ci, gc=gc, po=po, pi=pi)
