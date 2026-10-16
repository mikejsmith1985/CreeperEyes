import trimesh, numpy as np, manifold3d as mf
from scipy import ndimage
from skimage import measure
b = np.load('best.npz'); col, horn, yaw, pitch, du, dv = b['best']; O, u, v, w, lo, P = b['O'], b['u'], b['v'], b['w'], b['lo'], float(b['P'])
gu0, gu1, gv0, gv1 = b['geo']
occ = np.load('occ.npy')
SPK_D, CLR, WALL, POCKET, LIP, THROAT_R, FL = 40.0, 0.35, 2.2, 7.0, 1.6, 17.5, 6.0
R_out = SPK_D/2 + CLR + WALL
mouth_in  = (gu0 - 1.2, gu1 + 1.2, gv0 - 1.2, gv1 + 1.2)
mouth_out = (mouth_in[0] - WALL, mouth_in[1] + WALL, mouth_in[2] - WALL, mouth_in[3] + WALL)
flange    = (mouth_out[0] - FL + WALL, mouth_out[1] + FL - WALL, mouth_out[2] - FL + WALL, mouth_out[3] + FL - WALL)
Z0 = -6.0
su, sv = (gu0 + gu1)/2 + du, (gv0 + gv1)/2 + dv
def cyl(r, h, z0, seg=128): return mf.Manifold.cylinder(h, r, r, seg).translate([0, 0, z0])
def box(x0, x1, y0, y1, z0, z1): return mf.Manifold.cube([x1-x0, y1-y0, z1-z0]).translate([x0, y0, z0])
def spk(m): return m.rotate([pitch, yaw, 0]).translate([su, sv, horn])

body = mf.Manifold.hull(box(*mouth_out, Z0, 2.5) + spk(cyl(R_out, 0.5, 0)))
# flange with a 45 deg chamfer underneath (no overhang when printed mouth-up)
K = FL - WALL + 2.0
inset = (flange[0] + K, flange[1] - K, flange[2] + K, flange[3] - K)
body = body + mf.Manifold.hull(box(*flange, Z0, 2.0) + box(*inset, Z0, 2.0 + K)) + spk(cyl(R_out, LIP + POCKET, 0))
chan = mf.Manifold.hull(box(*mouth_in, Z0 - 1, 0.6) + spk(cyl(THROAT_R, 0.1, -0.05)))
# throat: 0.5 mm flat seat for the speaker rim, then a 45 deg cone in to the throat (no flat ceiling)
SEAT = 0.5; rs = SPK_D/2 + CLR - SEAT; ch = rs - THROAT_R
chan = chan + spk(cyl(THROAT_R, 0.2, -0.1)) + spk(mf.Manifold.cylinder(ch + 0.02, THROAT_R, rs, 128).translate([0, 0, LIP - ch]))
pocket = spk(cyl(SPK_D/2 + CLR, POCKET + 1, LIP))
# solder-tab / wire slot with a pointed 45 deg roof
notch = spk(mf.Manifold.hull(box(-6, 6, 14, 26, LIP - 0.5, LIP + POCKET + 1) + box(-0.05, 0.05, 14, 26, LIP - 6.5, LIP - 0.5)))
part = body - chan - pocket - notch
for a in (0, 120, 240):                                                # crush ribs
    r = SPK_D/2 + CLR - 0.25
    rib = cyl(0.6, POCKET - 1, LIP + 1, 24).translate([r*np.cos(np.radians(a)), r*np.sin(np.radians(a)), 0])
    part = part + spk(rib - cyl(SPK_D/2 + CLR - 0.35, POCKET + 2, LIP))

# bust shell solid (dilated ~0.35 mm for a glue gap), as a mesh in local coords
solid = ndimage.binary_dilation(occ, iterations=1)
pad = np.pad(solid, 1)
verts, faces, _, _ = measure.marching_cubes(pad.astype(np.float32), 0.5)
verts = (verts - 1) * P + lo
bm = trimesh.Trimesh(verts, faces[:, ::-1], process=True)
trimesh.smoothing.filter_taubin(bm, iterations=25)                     # smooth the voxel steps on the glue face
if bm.volume < 0: bm.invert()
print('bust solid watertight', bm.is_watertight, 'vol %.1f cm3' % (bm.volume / 1000))
bmf = mf.Manifold(mf.Mesh(bm.vertices.astype(np.float32), bm.faces.astype(np.uint32)))
part = part - bmf
# keep only the main piece (trim can leave crumbs outside the wall)
pieces = part.decompose(); part = max(pieces, key=lambda p: p.volume())
pm = part.to_mesh(); tm = trimesh.Trimesh(pm.vert_properties[:, :3], pm.tri_verts)
print('part pieces', len(pieces), 'watertight', tm.is_watertight, 'volume %.1f cm3' % (tm.volume / 1000))
M = np.eye(4); M[:3, 0], M[:3, 1], M[:3, 2], M[:3, 3] = u, v, w, O
tw = tm.copy(); tw.apply_transform(M); tw.export('speaker_horn_in_bust_position.stl')
# print orientation: flat back of the speaker pocket on the bed, horn pointing up
pr = tm.copy()
pr.apply_transform(trimesh.transformations.rotation_matrix(np.pi, [1, 0, 0]))
pr.apply_translation(-pr.bounds[0]); pr.apply_translation([-(pr.bounds[1][0])/2, -(pr.bounds[1][1])/2, 0])
pr.export('CreeperEyes_SpeakerHorn.stl'); print('print size mm', (pr.bounds[1]-pr.bounds[0]).round(1))
# 5-minute fit-test ring for the speaker
ring = (cyl(R_out, POCKET, 0) - cyl(SPK_D/2 + CLR, POCKET + 1, -0.5) - box(-6, 6, 14, 26, -1, POCKET + 1))
for a in (0, 120, 240):
    r = SPK_D/2 + CLR - 0.25
    ring = ring + (cyl(0.6, POCKET, 0, 24).translate([r*np.cos(np.radians(a)), r*np.sin(np.radians(a)), 0]) - cyl(SPK_D/2 + CLR - 0.35, POCKET + 2, -1))
rm = ring.to_mesh(); trimesh.Trimesh(rm.vert_properties[:, :3], rm.tri_verts).export('CreeperEyes_SpeakerFitTest.stl')
bm.export('bust_shell_local.stl'); tm.export('part_local.stl')
