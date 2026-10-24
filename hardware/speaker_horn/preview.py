import trimesh, numpy as np
import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
part = trimesh.load('part_local.stl'); bust = trimesh.load('bust_shell_local.stl')
b = np.load('best.npz'); col, horn, yaw, pitch, du, dv = b['best']; gu0, gu1, gv0, gv1 = b['geo']
su, sv = (gu0+gu1)/2+du, (gv0+gv1)/2+dv
fig = plt.figure(figsize=(16, 6))
for i, (nrm, org, ax_a, ax_b, title) in enumerate([
        ([1, 0, 0], [su, 0, 0], 2, 1, 'Side cut (through speaker centre)'),
        ([0, 1, 0], [0, (gv0+gv1)/2, 0], 2, 0, 'Top cut (through grill centre)')]):
    ax = fig.add_subplot(1, 3, i+1)
    for mesh, col_, lw in ((bust, '#777', 1.2), (part, '#1f9d3a', 1.6)):
        sec = mesh.section(plane_origin=org, plane_normal=nrm)
        if sec is None: continue
        for e in sec.discrete: ax.plot(e[:, ax_a], e[:, ax_b], color=col_, lw=lw)
    ax.set_aspect('equal'); ax.set_title(title); ax.set_xlabel('depth into bust (mm)')
    ax.set_ylabel('up (mm)' if ax_b == 1 else 'across (mm)')
    ax.grid(alpha=.2)
ax = fig.add_subplot(1, 3, 3, projection='3d')
for mesh, fc, al in ((part, '#2fbf5a', 1.0),):
    m = mesh
    pc = Poly3DCollection(m.triangles, facecolor=fc, edgecolor='none', alpha=al)
    light = np.clip(m.face_normals @ np.array([0.4, 0.3, -0.85]), 0.15, 1)
    pc.set_facecolor([(0.18*l+0.05, 0.75*l+0.1, 0.35*l+0.05) for l in light]); ax.add_collection3d(pc)
ax.set_xlim(-30, 30); ax.set_ylim(-25, 40); ax.set_zlim(-8, 35); ax.view_init(elev=20, azim=-60)
ax.set_title('Horn (grill side at the bottom)')
plt.tight_layout(); plt.savefig('horn_preview.png', dpi=80)
