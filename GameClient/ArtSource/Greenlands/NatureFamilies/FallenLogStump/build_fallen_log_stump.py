"""Three forest deadwood variants: two fallen logs and one rooted stump."""
import json
import math
import sys
from pathlib import Path

import bpy
import numpy as np

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent))
import nature_kit as K
import rock_family as R
import veg_kit as G

h='Sources/pine_bark/pine_bark'
m='Sources/mossy_rock/mossy_rock'
BARK=dict(name='MI_FallenLogStump_Bark',base=h+'_Diffuse.jpg',
          normal=h+'_nor_gl.jpg',rough=h+'_Rough.jpg',uv_scale=[1.3,1.3],
          tint=[.72,.69,.61],sat=.72,
          blend=dict(base=m+'_Diffuse.jpg',normal=m+'_nor_gl.jpg',
                     rough=m+'_Rough.jpg',uv_scale=[3,3],
                     tint=[.37,.48,.19],sat=.9,amount=.75,contrast=.20))
WOOD=dict(name='MI_FallenLogStump_BrokenWood',base=h+'_Diffuse.jpg',
          normal=h+'_nor_gl.jpg',rough=h+'_Rough.jpg',
          tint=[3.15,2.40,1.55],sat=.42)
RING=dict(name='MI_FallenLogStump_EndGrain',base=h+'_Diffuse.jpg',
          normal=h+'_nor_gl.jpg',rough=h+'_Rough.jpg',
          tint=[1.35,1.0,.7],sat=.48)

def make_log(length,radius,seed):
    rng=np.random.default_rng(seed); rings=76; sides=32
    v=[];t=[];uv=[];mi=[]
    phase=rng.uniform(0,6.28,5)
    for j in range(rings):
        s=j/(rings-1);x=(s-.5)*length
        taper=1-.27*s+.055*math.sin(s*8+phase[0])
        centre_y=.27*math.sin(s*math.pi*1.5+phase[4])*s
        centre_z=radius-.065+.11*math.sin(s*math.pi*1.7)
        for k in range(sides):
            a=2*math.pi*k/sides
            lobe=1+.045*math.sin(a*5+phase[1]+s*2)+.018*math.sin(a*11+s*14+phase[2])
            rr=radius*taper*lobe
            edge=0
            if j in (0,1,rings-2,rings-1):
                edge=(.035*math.sin(a*5+phase[3])+.035*math.sin(a*9+phase[1]))*(1 if j in (0,rings-1) else .35)
            v.append((x+edge,centre_y+rr*math.cos(a),centre_z+rr*math.sin(a)))
    for j in range(rings-1):
        for k in range(sides):
            a=j*sides+k;b=j*sides+(k+1)%sides
            c=(j+1)*sides+(k+1)%sides;d=(j+1)*sides+k
            u0=k/sides*2.1;u1=(k+1)/sides*2.1
            v0=j/(rings-1)*length/1.8;v1=(j+1)/(rings-1)*length/1.8
            t.extend(((a,b,c),(a,c,d)))
            uv.extend((((u0,v0),(u1,v0),(u1,v1)),((u0,v0),(u1,v1),(u0,v1))))
            p1=15<=j<=28 and abs(k-12)<=(1 if 19<=j<=24 else 0)
            p2=42<=j<=56 and abs(k-13)<=(1 if 47<=j<=51 else 0)
            peeled=p1 or p2
            mi.extend((2,2) if peeled else (0,0))
    # Thin bark lips lift along two exposed peeling patches.
    for j0,j1,k in ((15,28,14),(42,56,15)):
        for j in range(j0,j1):
            edge=[]
            for jj,kk,lift in ((j,k,0),(j,k+1,.027),(j+1,k+1,.04),(j+1,k,0)):
                base=np.array(v[jj*sides+kk],np.float32)
                a=2*math.pi*kk/sides
                base+=np.array((0,math.cos(a)*lift,math.sin(a)*lift))
                edge.append(len(v));v.append(tuple(base))
            for face in ((edge[0],edge[1],edge[2]),(edge[0],edge[2],edge[3])):
                t.append(face);uv.append(((0,j/8),(1,j/8),(1,(j+1)/8)));mi.append(0)
    # Recessed pale splintered cores. Each end is a ragged annulus and fan.
    for end,reverse in ((0,True),(rings-1,False)):
        x=(-.5 if end==0 else .5)*length
        start=len(v)
        cy=.27*math.sin((0 if reverse else 1)*math.pi*1.5+phase[4])*(0 if reverse else 1)
        cz=radius-.065+.11*math.sin((0 if reverse else 1)*math.pi*1.7)
        for k in range(sides):
            a=2*math.pi*k/sides;outer=v[end*sides+k]
            inset=.025+.035*(.5+.5*math.sin(a*9+phase[0]))
            v.append((outer[0]+(inset if reverse else -inset),
                      cy+.82*(outer[1]-cy),cz+.82*(outer[2]-cz)))
        center=len(v);v.append((x+(.035 if reverse else -.035),cy,cz))
        for k in range(sides):
            kn=(k+1)%sides;outer=end*sides+k;outer2=end*sides+kn
            inner=start+k;inner2=start+kn
            faces=((outer,outer2,inner2),(outer,inner2,inner),(inner,inner2,center))
            for face in faces:
                t.append(face[::-1] if reverse else face)
                uv.append(tuple(((v[q][1]/radius+1)*.5,(v[q][2]/radius)*.5) for q in face))
                mi.append(1)
        # Round, longitudinal wood fibres project beyond the ragged bark collar.
        for k in (2,8,15,23,29):
            outer=np.array(v[end*sides+k],np.float32)
            inward=np.array((0,cy-outer[1],cz-outer[2]),np.float32)*.18
            direction=-1 if reverse else 1
            extent=.12+.025*((k*7)%4)
            p=np.array((outer+inward,
                        outer+inward+np.array((direction*extent*.55,0,.012)),
                        outer+inward+np.array((direction*extent,.012,.015))),np.float32)
            rv,rt,ru,_=G.tube(p,np.array([.055,.037,.004],np.float32),8)
            off=len(v);v.extend(rv.tolist());t.extend((rt+off).tolist())
            uv.extend(ru.tolist());mi.extend([1]*len(rt))
    # Broken branch bases interrupt the otherwise regular trunk silhouette.
    for side,s in ((-1,.22),(1,.43),(-1,.66),(1,.82)):
        x=(s-.5)*length
        y=.27*math.sin(s*math.pi*1.5+phase[4])*s+side*radius*.77
        z=radius-.065+.11*math.sin(s*math.pi*1.7)+radius*.12
        p=np.array([(x,y,z),(x+.05,y+side*.20,z+.12),
                    (x+.15,y+side*.38,z+.18),(x+.19,y+side*.43,z+.17)],np.float32)
        rv,rt,ru,_=G.tube(p,np.array([.17,.14,.105,.09],np.float32),14)
        off=len(v);v.extend(rv.tolist());t.extend((rt+off).tolist())
        uv.extend(ru.tolist());mi.extend([0]*len(rt))
        cap=len(v);v.append(tuple(p[-1]+np.array([.025,side*.012,0])))
        for k in range(14):
            face=(off+3*14+k,off+3*14+(k+1)%14,cap)
            t.append(face);uv.append(((0,0),(1,0),(.5,.5)));mi.append(1)
    return np.array(v,np.float32),np.array(t,np.int32),np.array(uv,np.float32),np.array(mi,np.int32)

def make_stump(seed):
    rng=np.random.default_rng(seed); sides=32;rings=32
    v=[];t=[];uv=[];mi=[];height=1.15;radius=.56
    phase=rng.uniform(0,6.28)
    for j in range(rings):
        s=j/(rings-1);z=-.13+height*s
        for k in range(sides):
            a=2*math.pi*k/sides
            rr=radius*(1.18-.24*s)*(1+.11*math.sin(5*a+phase)+.035*math.sin(13*a+s*12))
            zz=z+(.045*math.sin(5*a+phase)+.02*math.sin(11*a))*(s**8)
            v.append((rr*math.cos(a),rr*math.sin(a),zz))
    for j in range(rings-1):
        for k in range(sides):
            a=j*sides+k;b=j*sides+(k+1)%sides;c=(j+1)*sides+(k+1)%sides;d=(j+1)*sides+k
            t.extend(((a,b,c),(a,c,d)));mi.extend((0,0))
            uv.extend((((k/sides*2,j/rings),((k+1)/sides*2,j/rings),((k+1)/sides*2,(j+1)/rings)),
                       ((k/sides*2,j/rings),((k+1)/sides*2,(j+1)/rings),(k/sides*2,(j+1)/rings))))
    top=(rings-1)*sides
    last=[top+k for k in range(sides)]
    for band,frac in enumerate((.90,.86,.75,.71,.60,.56,.45,.41,.30,.26,.15,.11)):
        current=[]
        for k in range(sides):
            a=2*math.pi*k/sides;idx=len(v);current.append(idx)
            rr=radius*frac*(1+.025*math.sin(6*a+phase))
            v.append((rr*math.cos(a),rr*math.sin(a),height-.10+.01*math.sin(7*a+band)))
        for k in range(sides):
            kn=(k+1)%sides
            for face in ((last[k],last[kn],current[kn]),(last[k],current[kn],current[k])):
                t.append(face);mi.append(1 if band%2==0 else 2)
                uv.append(tuple(((v[q][0]/radius+1)*.5,(v[q][1]/radius+1)*.5) for q in face))
        last=current
    center=len(v);v.append((0,0,height-.1))
    for k in range(sides):
        face=(last[k],last[(k+1)%sides],center)
        t.append(face);mi.append(1)
        uv.append(tuple(((v[q][0]/radius+1)*.5,(v[q][1]/radius+1)*.5) for q in face))
    parts=[(np.array(v,np.float32),np.array(t,np.int32),np.array(uv,np.float32),np.array(mi,np.int32))]
    # Six separate barked roots flare out, curve, and dive into soil.
    for i in range(6):
        a=2*math.pi*i/6+rng.uniform(-.17,.17);reach=rng.uniform(.78,1.3)
        q=np.linspace(0,1,23)
        theta=a+.18*np.sin(q*math.pi)*(1 if i%2 else -1)
        distance=.38+reach*q
        p=np.c_[distance*np.cos(theta),distance*np.sin(theta),
                .34*(1-q)**2+.08*np.sin(q*math.pi)-.24*q**3]
        r=(.19+.025*rng.random())*(1-q)**.72+.025
        rv,rt,ru,_=G.tube(p,r,16,mark_phase=seed+i)
        parts.append((rv,rt,ru,np.zeros(len(rt),np.int32)))
    verts=[];faces=[];uvs=[];mats=[];offset=0
    for pv,pt,pu,pm in parts:
        verts.append(pv);faces.append(pt+offset);uvs.append(pu);mats.append(pm);offset+=len(pv)
    return np.vstack(verts),np.vstack(faces),np.vstack(uvs),np.concatenate(mats)

K.reset();K.render_setup((2200,1050),48);K.add_ground(colour=(.18,.19,.14))
mats=[K.build_material(BARK),K.build_material(WOOD),K.build_material(RING)]
objects=[];rows=[]
for i,(kind,length,radius,seed) in enumerate((('fallen_log',5.1,.42,19),('fallen_log',6.35,.35,41),('rooted_stump',0,0,73))):
    v,t,uv,mi=make_log(length,radius,seed) if length else make_stump(seed)
    name=f'SM_FallenLogStump_{i:02d}'
    obj=K.make(name,v,t,uv,mi,mats)
    moss=K.moss_mask(obj,seed=seed,low=.25,high=.68,breakup=.65)
    a=obj.data.color_attributes['Col'];col=np.empty(len(obj.data.vertices)*4,np.float32)
    a.data.foreach_get('color',col.ravel());col=col.reshape(-1,4)
    xyz=K.verts(obj)
    patches=.5+.5*np.sin(xyz[:,0]*3.4+seed)*np.sin(xyz[:,1]*7.3+xyz[:,2]*4.2)
    col[:,0]*=np.clip((patches-.28)/.32,0,1)
    if length:
        contact=np.clip((.14-xyz[:,2])/.20,0,1)
        col[:,0]=np.maximum(col[:,0],contact*(.22+.20*patches))
    a.data.foreach_set('color',col.ravel());moss=float(col[:,0].mean())
    objects.append(obj)
    rows.append(dict(mesh=name,kind=kind,seed=seed,burial_m=round(float(-v[:,2].min()),3),
                     moss_mean=round(moss,3),materials=[BARK['name'],WOOD['name'],RING['name']]))
cfg=dict(family='FallenLogStump',dir=str(HERE),prefix='SM_FallenLogStump',
         asset_id='VEG-GreenlandsFallenLogStump-01',role='fallen_log_stump',
         budget=40000,nanite=True,gap=1.25,res=(2200,1050),look_z=.62,
         scale_note='Two fallen logs 5.1 and 6.35 m long, plus rooted stump; buried base below z=0.',
         extra_sources=['mossy_rock','pine_bark'])
R.finish(cfg,objects,rows,[BARK,WOOD,RING],['pine_bark','mossy_rock'])
# Show the broken ends and bends in the lineup, with the same 1.8 m marker.
sc=bpy.context.scene;sc.render.resolution_x,sc.render.resolution_y=2200,1050
K.camera((17.0,-18,4.1),(8.3,0,.5),lens=30)
K.render(HERE/'FallenLogStump-Variants.png')
sc=bpy.context.scene;sc.render.resolution_x,sc.render.resolution_y=1500,1150
ox=objects[0].location.x
K.camera((ox+4.6,-1.8,1.6),(ox+2.15,0,.48),lens=32)
for o in objects[1:]:o.hide_render=True
for o in bpy.data.objects:
    if o.name.startswith('RENDER_ONLY_scale_1.8m'):o.hide_render=True
K.render(HERE/'FallenLogStump-Detail-3m.png')
objects[0].hide_render=True;objects[2].hide_render=False
sx=objects[2].location.x
K.camera((sx+1.4,-2.55,1.6),(sx,0,.63),lens=35)
K.render(HERE/'FallenLogStump-Stump-3m.png')
for o in objects:o.hide_render=False
for o in bpy.data.objects:
    if o.name.startswith('RENDER_ONLY_scale_1.8m'):o.hide_render=False
bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'FallenLogStump.blend'))
p=HERE/'asset-manifest.json';man=json.loads(p.read_text())
man['generator']='build_fallen_log_stump.py'
man['renders']=['FallenLogStump-Variants.png','FallenLogStump-Variants-Top.png','FallenLogStump-Detail-3m.png','FallenLogStump-Stump-3m.png']
man['units']='metres, Z up, front -Y, origin at ground contact; buried base below z=0'
man['concept_reference']='docs/Game-Design/concept-art/UI/v5/gameplay-hud.png'
for r in man['variants']:r['origin']='ground contact, footprint centre; base below z=0'
assert len(man['variants'])==3 and max(man['triangles'])<=40000
assert 4<=man['variants'][0]['size_m'][0]<=7 and 4<=man['variants'][1]['size_m'][0]<=7
p.write_text(json.dumps(man,indent=2)+'\n')
