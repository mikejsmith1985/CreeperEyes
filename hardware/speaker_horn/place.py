# Horn v5: placement search against the REAL printed bust solid (incl. the 45 deg shoulder slab)
import numpy as np, trimesh, manifold3d as mf, itertools, time
from matplotlib.path import Path
t0=time.time()
b=np.load('best.npz'); O,u,v,w=b['O'],b['u'],b['v'],b['w']; gu0,gu1,gv0,gv1=b['geo']
bust=trimesh.load('/mnt/user-data/uploads/frankenstein_stls/CreeperEyes_NeckShoulders_paintready_nostitches.stl')
BM=mf.Manifold(mf.Mesh(vert_properties=bust.vertices.astype(np.float32), tri_verts=bust.faces.astype(np.uint32)))
box=mf.Manifold.cube([110,95,90]).translate([5,-90,-5])
BC=BM^box                                                   # bust near the grill (exact)
import os
# world occupancy grid by slicing
H=0.4; lo=np.array([5.,-90.,0.]); xs=np.arange(5,115,H); ys=np.arange(-90,5,H); zs=np.arange(0,85,H)
X,Y=np.meshgrid(xs,ys,indexing='ij'); pts=np.c_[X.ravel(),Y.ravel()]
occ=np.zeros((len(xs),len(ys),len(zs)),bool)
if os.path.exists('occ_world.npz'): occ=np.load('occ_world.npz')['occ']
for k,z in (enumerate(zs) if not occ.any() else []):
    ins=np.zeros(len(pts),bool)
    for p in BC.slice(z+1e-3).to_polygons():
        if len(p)>2: ins^=Path(p).contains_points(pts)
    occ[:,:,k]=ins.reshape(X.shape)
print('occ', occ.sum(), round(time.time()-t0),'s', flush=True)
np.savez_compressed('occ_world.npz', occ=occ, lo=lo, H=H)
def to_world(Lp): return O[None]+Lp[:,:1]*u+Lp[:,1:2]*v+Lp[:,2:3]*w
def occupied(Lp):
    W=to_world(Lp); idx=np.floor((W-lo)/H).astype(int)
    ok=np.all((idx>=0)&(idx<occ.shape),axis=1); out=np.zeros(len(W),bool)
    out[ok]=occ[idx[ok,0],idx[ok,1],idx[ok,2]]; return out
SPK_D,CLR,WALL,POCKET,LIP,THROAT_R,FL=40.0,0.35,2.2,7.0,1.6,17.5,6.0
R_out=SPK_D/2+CLR+WALL
mouth_in=(gu0-1.2,gu1+1.2,gv0-1.2,gv1+1.2); mouth_out=(mouth_in[0]-WALL,mouth_in[1]+WALL,mouth_in[2]-WALL,mouth_in[3]+WALL)
cu,cv=(gu0+gu1)/2,(gv0+gv1)/2
def rot(p,yaw,pitch=0):
    a=np.radians(yaw); c,s=np.cos(a),np.sin(a)
    p=np.stack([p[...,0]*c+p[...,2]*s,p[...,1],-p[...,0]*s+p[...,2]*c],-1)
    bb=np.radians(pitch); c,s=np.cos(bb),np.sin(bb)
    return np.stack([p[...,0],p[...,1]*c-p[...,2]*s,p[...,1]*s+p[...,2]*c],-1)
rng=np.random.default_rng(2)
N=5000
rr=np.sqrt(rng.random(N))*(R_out+1.0); aa=rng.random(N)*2*np.pi; zz=rng.random(N)*(LIP+POCKET+1.0)
RING=np.stack([rr*np.cos(aa),rr*np.sin(aa),zz],1)
MO=np.stack([rng.uniform(mouth_out[0],mouth_out[1],N),rng.uniform(mouth_out[2],mouth_out[3],N),np.zeros(N)],1)
ra=np.sqrt(rng.random(N))*(R_out+1.0); ta=rng.random(N)*2*np.pi; DISK=np.stack([ra*np.cos(ta),ra*np.sin(ta),np.zeros(N)],1)
FR=rng.random((N,1))
from scipy.spatial import ConvexHull
ang=np.linspace(0,2*np.pi,48,endpoint=False)
def rect(r,z): return np.array([[r[0],r[2],z],[r[1],r[2],z],[r[1],r[3],z],[r[0],r[3],z]])
def hull_overhang(P,up,inner):
    hh=ConvexHull(P); A=0.0
    for simp,eq in zip(hh.simplices,hh.equations):
        nz=eq[:3]@up
        if inner: nz=-nz
        if -0.995 < nz < -0.72:
            p=P[simp]; A+=0.5*np.linalg.norm(np.cross(p[1]-p[0],p[2]-p[0]))
    return A
def overhang(su,sv,yaw,pitch,horn):
    c=np.array([su,sv,horn]); d=rot(np.array([[0,0,1.0]]),yaw,pitch)[0]; up=-d
    Ro=rot(np.stack([R_out*np.cos(ang),R_out*np.sin(ang),np.zeros(48)],1),yaw,pitch)+c
    Rt=rot(np.stack([THROAT_R*np.cos(ang),THROAT_R*np.sin(ang),np.zeros(48)],1),yaw,pitch)+c
    return hull_overhang(np.r_[rect(mouth_out,0),Ro],up,False)+hull_overhang(np.r_[rect(mouth_in,0),Rt],up,True)
def score(su,sv,yaw,pitch,horn):
    c=np.array([su,sv,horn])
    ring=rot(RING,yaw,pitch)+c
    W=to_world(ring)
    if W[:,2].min()<1.0: return None                      # keep above the table
    body=MO*(1-FR)+(rot(DISK,yaw,pitch)+c)*FR
    body=body[body[:,2]>4.0]                              # ignore the part that meets the wall
    Wb=to_world(body)
    if Wb[:,2].min()<1.0: return None
    return occupied(ring).sum()+occupied(body).sum()
best=None; cands=[]
for horn,yaw,pitch,du,dv in itertools.product((10,13,16,19,22,25,28),(-30,-20,-10,0,10),(-20,-10,0,10,20),range(-30,9,3),range(-6,25,3)):
    sc=score(cu+du,cv+dv,yaw,pitch,horn)
    if sc is None: continue
    tot=sc*100+overhang(cu+du,cv+dv,yaw,pitch,horn)*0.2+abs(du)*0.3+abs(dv)*0.3+abs(yaw)*0.05+abs(pitch)*0.05+horn*0.3
    if sc==0: cands.append((overhang(cu+du,cv+dv,yaw,pitch,horn),horn,yaw,pitch,du,dv))
    if best is None or tot<best[0]: best=(tot,sc,horn,yaw,pitch,du,dv)
print('BEST',best,round(time.time()-t0),'s')
cands.sort(); np.save('cands.npy',np.array(cands)); print(len(cands), cands[:10]); np.save('best5.npy',np.array(best[1:])); print('overhang est', overhang(cu+best[5],cv+best[6],best[3],best[4],best[2]))
