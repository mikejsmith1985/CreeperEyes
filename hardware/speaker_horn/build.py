import trimesh, numpy as np, manifold3d as mf
b=np.load('best.npz'); O,u,v,w=b['O'],b['u'],b['v'],b['w']; gu0,gu1,gv0,gv1=b['geo']
col,horn,yaw,pitch,du,dv=np.load('best5.npy')
SPK_D,CLR,WALL,POCKET,LIP,THROAT_R,FL=40.0,0.35,2.2,7.0,1.6,17.5,6.0
R_out=SPK_D/2+CLR+WALL
mouth_in=(gu0-1.2,gu1+1.2,gv0-1.2,gv1+1.2); mouth_out=(mouth_in[0]-WALL,mouth_in[1]+WALL,mouth_in[2]-WALL,mouth_in[3]+WALL)
flange=(mouth_out[0]-FL+WALL,mouth_out[1]+FL-WALL,mouth_out[2]-FL+WALL,mouth_out[3]+FL-WALL)
Z0=-6.0; su,sv=(gu0+gu1)/2+du,(gv0+gv1)/2+dv
def cyl(r,h,z0,seg=128): return mf.Manifold.cylinder(h,r,r,seg).translate([0,0,z0])
def box(x0,x1,y0,y1,z0,z1): return mf.Manifold.cube([x1-x0,y1-y0,z1-z0]).translate([x0,y0,z0])
def spk(m): return m.rotate([pitch,yaw,0]).translate([su,sv,horn])
K=FL-WALL+2.0; inset=(flange[0]+K,flange[1]-K,flange[2]+K,flange[3]-K)
body=mf.Manifold.hull(box(*mouth_out,Z0,2.5)+spk(cyl(R_out,0.5,0)))
body=body+mf.Manifold.hull(box(*flange,Z0,2.0)+box(*inset,Z0,2.0+K))+spk(cyl(R_out,LIP+POCKET,0))
chan=mf.Manifold.hull(box(*mouth_in,Z0-1,0.6)+spk(cyl(THROAT_R,0.1,-0.05)))
SEAT=0.5; rs=SPK_D/2+CLR-SEAT; ch=rs-THROAT_R
chan=chan+spk(cyl(THROAT_R,0.2,-0.1))+spk(mf.Manifold.cylinder(ch+0.02,THROAT_R,rs,128).translate([0,0,LIP-ch]))
pocket=spk(cyl(SPK_D/2+CLR,POCKET+1,LIP))
notch=spk(mf.Manifold.hull(box(-6,6,14,26,LIP-0.5,LIP+POCKET+1)+box(-0.05,0.05,14,26,LIP-6.5,LIP-0.5)))
part=body-chan-pocket-notch
for a in (0,120,240):
    r=SPK_D/2+CLR-0.25
    rib=cyl(0.6,POCKET-1,LIP+1,24).translate([r*np.cos(np.radians(a)),r*np.sin(np.radians(a)),0])
    part=part+spk(rib-cyl(SPK_D/2+CLR-0.35,POCKET+2,LIP))
# exact bust solid, moved into the local frame, grown ~0.3 mm for a glue gap (3 shifted copies)
bust=trimesh.load('/mnt/user-data/uploads/frankenstein_stls/CreeperEyes_NeckShoulders_paintready_nostitches.stl')
BM=mf.Manifold(mf.Mesh(vert_properties=bust.vertices.astype(np.float32),tri_verts=bust.faces.astype(np.uint32)))^mf.Manifold.cube([110,95,90]).translate([5,-90,-5])
M=np.eye(4); M[:3,0],M[:3,1],M[:3,2],M[:3,3]=u,v,w,O; Mi=np.linalg.inv(M)
BL=BM.transform(Mi[:3,:4])
part=part-BL
for d in ([0,0,0.3],[0,0.25,0.15],[0.25,0,0.15],[-0.25,0,0.15],[0,-0.25,0.15]): part=part-BL.translate(d)
pieces=part.decompose(); part=max(pieces,key=lambda p:p.volume())
pm=part.to_mesh(); tm=trimesh.Trimesh(pm.vert_properties[:,:3],pm.tri_verts)
print('pieces',len(pieces),'watertight',tm.is_watertight,'vol cm3',round(tm.volume/1000,1),'collision check cm3',round((part.transform(M[:3,:4])^BM).volume()/1000,3))
tw=tm.copy(); tw.apply_transform(M); tw.export('speaker_horn_in_bust_position.stl')
# print orientation: speaker back face down on the bed.
# NOTE: the local (u,v,w) frame is LEFT-handed (det = -1), so the local-frame part is a mirror image.
# Always derive the print file from the part in bust coordinates (tw) with a proper rotation.
d=np.stack([u,v,w],1)@(trimesh.transformations.euler_matrix(np.radians(pitch),np.radians(yaw),0,'sxyz')[:3,:3]@[0,0,1.0])
R=trimesh.geometry.align_vectors(d,[0,0,-1.0]); assert np.linalg.det(R[:3,:3])>0
pr=tw.copy(); pr.apply_transform(R); pr.apply_translation(-pr.bounds[0]); pr.apply_translation([-(pr.bounds[1][0])/2,-(pr.bounds[1][1])/2,0])
pr.export('CreeperEyes_SpeakerHorn.stl'); print('print size',(pr.bounds[1]-pr.bounds[0]).round(1))
n=pr.face_normals; a=pr.area_faces; c=pr.triangles_center
for lim in (45,50,60):
    bad=(n[:,2]<-np.sin(np.radians(lim)))&(c[:,2]>0.3); print('overhang >',lim,'deg from vertical mm2',round(a[bad].sum(),1))
bad=(n[:,2]<-np.sin(np.radians(46)))&(c[:,2]>0.3)
h=np.histogram(c[bad,2],bins=10,weights=a[bad]); print(np.c_[h[1][:-1].round(1),h[0].round(1)])
