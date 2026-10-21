"""Shared tree/leaf helpers for the small-vegetation families (saplings, herbs, litter): tube(), skeleton(), leaf_library(), rot(), leaves().
Extracted unchanged from BroadleafYoung/build_broadleaf.py (leaf geometry harvested from CC0 Poly Haven tree_small_02)."""
import sys,math
import numpy as np
from pathlib import Path
import nature_kit as K,bpy
LEAF_BUDGET=260000
BARK_TILE=1.89
DROP=1.0;TW=.8


def smooth(x):
    x=np.clip(x,0,1);return x*x*(3-2*x)


def tube(P,r,sides,mark_phase=None,thin_brown=True):
    """Tube along polyline P (n,3) with radii r (n,). Returns v,t,uv,vcolR."""
    n=len(P);T=np.gradient(P,axis=0);T/=np.linalg.norm(T,axis=1)[:,None]+1e-9
    ref=np.where(np.abs(T[:,2:3])>.9,np.array([[1,0,0]]),np.array([[0,0,1]]));N=np.cross(T,ref);N/=np.linalg.norm(N,axis=1)[:,None]+1e-9;B=np.cross(T,N)
    th=np.linspace(0,2*np.pi,sides,endpoint=False);ring=(np.cos(th)[None,:,None]*N[:,None,:]+np.sin(th)[None,:,None]*B[:,None,:])*r[:,None,None]+P[:,None,:]
    v=ring.reshape(-1,3);idx=np.arange(n*sides).reshape(n,sides);tris=[];uvs=[]
    s=np.r_[0,np.cumsum(np.linalg.norm(np.diff(P,axis=0),axis=1))]
    for i in range(n-1):
        for k in range(sides):
            k2=(k+1)%sides;a,b,c,d=idx[i,k],idx[i,k2],idx[i+1,k2],idx[i+1,k]
            c0=2*np.pi*r[i]/BARK_TILE;c1=2*np.pi*r[i+1]/BARK_TILE;u0=k/sides;u1=(k+1)/sides
            tris+=[(a,b,c),(a,c,d)]
            uvs+=[((u0*c0,s[i]/BARK_TILE),(u1*c0,s[i]/BARK_TILE),(u1*c1,s[i+1]/BARK_TILE)),((u0*c0,s[i]/BARK_TILE),(u1*c1,s[i+1]/BARK_TILE),(u0*c1,s[i+1]/BARK_TILE))]
    t=np.array(tris,np.int64);uv=np.array(uvs,np.float32)
    ang=np.tile(th,n);z=v[:,2];ph=mark_phase or 0
    val=np.sin(z*34+4*np.sin(ang*2+ph)+np.repeat(np.sin(np.arange(n)*1.7+ph),sides)*1.5);marks=smooth((val-.6)/.15)*(np.sin(ang*3+ph*2)>-.85)
    marks=marks*np.repeat(smooth((r-.02)/.02),sides)
    R=np.maximum(marks,np.repeat(1-smooth((r-.008)/.014),sides)) if thin_brown else marks
    return v.astype(np.float32),t,uv,R.astype(np.float32)


def skeleton(vc):
    """Returns list of (points, radii, sides) wood tubes and leaf attach points (m,3)."""
    rng=np.random.default_rng(vc['seed']);tubes=[];attach=[]
    for si,(sx,sy,lx,ly,H,r0) in enumerate(vc['stems']):
        t=np.linspace(0,1,44);ph=rng.uniform(0,6.28,2)
        P=np.c_[sx+lx*t**2*1.0+.18*np.sin(t*4.2+ph[0])*t,sy+ly*t**2+.18*np.sin(t*3.6+ph[1])*t,H*t]
        r=r0*(1-t)**.85+.012;tubes.append((P,r,8))
        nb=int(rng.integers(17,22));tb=np.sort(rng.uniform(.2,.96,nb));golden=rng.uniform(0,6.28)
        for bi,tt in enumerate(tb):
            i=int(tt*43);p0=P[i];phi=golden+bi*2.39996
            crown=np.clip(np.sin(math.pi*(tt-.12)/.9),0,1)**.8;L=vc['lmax']*crown*(.75+.5*rng.random())+.25
            alpha=math.radians(np.interp(tt,[.2,1],[78,32])+rng.uniform(-8,8));drop=np.interp(tt,[.2,1],[1.2,.85])*DROP
            s=np.linspace(0,1,10)*L;horiz=np.array([math.cos(phi),math.sin(phi),0])
            Q=p0+horiz[None,:]*(np.sin(alpha)*s)[:,None]+np.array([0,0,1])[None,:]*(np.cos(alpha)*s-drop*s**2/max(L,.5)*.55)[:,None]
            rb=np.minimum(.4*r[i],.045)*(1-np.linspace(0,1,10))**.8+.006;tubes.append((Q,rb,5));attach+=list(Q[6:])+[Q[-1]]*8+[Q[-2]]*4
            for _ in range(int(L*5)):
                k=int(rng.integers(3,9));ph2=rng.uniform(0,6.28);dirb=Q[min(k+1,9)]-Q[k];dirb/=np.linalg.norm(dirb)
                side=np.cross(dirb,[0,0,1]);side/=np.linalg.norm(side)+1e-9;up=np.cross(side,dirb)
                d=math.cos(ph2)*side+math.sin(ph2)*up;ang=math.radians(rng.uniform(45,70));dd=dirb*math.cos(ang)+d*math.sin(ang)
                Lt=rng.uniform(.35,.9)*(1-.35*k/10);u=np.linspace(0,1,5)*Lt;W=Q[k]+dd[None,:]*u[:,None]+np.array([0,0,-1])[None,:]*(TW*u**2)[:,None]
                tubes.append((W,np.linspace(.0055,.002,5),4));attach+=list(W[2:])+[W[-1]]*5+[W[-1]+np.array([0,0,-.1])]*2
    return tubes,np.array(attach)


def leaf_library():
    objs=K.load_gltf('tree_small_02');o=max(objs.values(),key=lambda x:len(x.data.polygons))
    names=[m.name for m in o.data.materials];v,t,uv,mi=K.arrays(o);li=[i for i,n in enumerate(names) if 'leaves' in n][0];bpy.data.objects.remove(o,do_unlink=True);bpy.ops.outliner.orphans_purge(do_recursive=True)
    lt=t[mi==li];luv=uv[mi==li];used=np.unique(lt);rm=-np.ones(len(v),np.int64);rm[used]=np.arange(len(used));lv=v[used];lt=rm[lt]
    c=K.components(lt,len(lv));cid,inv,cnt=np.unique(c,return_inverse=True,return_counts=True);lib=[];rng=np.random.default_rng(5)
    for k in rng.permutation(np.where((cnt>=16)&(cnt<=50))[0])[:60]:
        tri=np.where(inv==k)[0];tv=lt[tri];u,ix=np.unique(tv,return_inverse=True);p=lv[u];ctr=p.mean(0);lib.append((p-ctr,ix.reshape(-1,3),luv[tri]))
    return lib


def rot(rng):
    a,b,c=rng.uniform(0,2*np.pi,3);Rz=np.array([[math.cos(a),-math.sin(a),0],[math.sin(a),math.cos(a),0],[0,0,1]]);Rx=np.array([[1,0,0],[0,math.cos(b),-math.sin(b)],[0,math.sin(b),math.cos(b)]])
    Rz2=np.array([[math.cos(c),-math.sin(c),0],[math.sin(c),math.cos(c),0],[0,0,1]]);return Rz@Rx@Rz2


def leaves(attach,lib,scale,seed):
    rng=np.random.default_rng(seed);V=[];T=[];UV=[];off=0;tris=0
    for p in attach[rng.permutation(len(attach))]:
        for _ in range(4):
            lv,lt,luv=lib[rng.integers(len(lib))];s=scale*rng.uniform(.8,1.25);w=(lv@rot(rng).T)*s+p+rng.normal(0,.05,3)+np.array([0,0,-.03])
            V.append(w);T.append(lt+off);UV.append(luv);off+=len(w);tris+=len(lt)
        if tris>=LEAF_BUDGET:break
    return np.vstack(V).astype(np.float32),np.vstack(T),np.concatenate(UV).astype(np.float32)




def flat_leaf(p):
    """Rotate a harvested leaf piece so its normal is +z, long axis +x, base at x=0. Returns (points, long length)."""
    c=p-p.mean(0);u,s,vt=np.linalg.svd(c,full_matrices=False);R=vt.copy()
    if np.linalg.det(R)<0:R[2]*=-1
    q=c@R.T;q[:,0]-=q[:,0].min();return q,float(q[:,0].max())
