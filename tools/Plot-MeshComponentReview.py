"""CPU-only orthographic mesh diagrams for explicitly selected component candidates."""
import argparse,json
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFont
p=argparse.ArgumentParser();p.add_argument('--report',required=True);p.add_argument('--object-index',type=int,default=0);p.add_argument('--component',type=int,action='append',required=True);p.add_argument('--output',required=True)
p.add_argument('--previous-report',help='Outline previous candidate locations over the current mesh, without coloring new component IDs')
a=p.parse_args();report_path=Path(a.report);report=json.loads(report_path.read_text());entry=report['objects'][a.object_index]
data=np.load(report_path.parent/entry['selection_file']);points=data['vertices'];triangles=data['triangles'];labels=data['component_per_vertex'];face_labels=labels[triangles[:,0]]
target=Path(a.output);target.mkdir(parents=True,exist_ok=False)
reference_points=points;reference_labels=labels
if a.previous_report:
    old_path=Path(a.previous_report);old_report=json.loads(old_path.read_text());old=old_report['objects'][a.object_index]
    old_data=np.load(old_path.parent/old['selection_file']);reference_points=old_data['vertices'];reference_labels=old_data['component_per_vertex']
font_path='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
font=lambda size:ImageFont.truetype(font_path,size) if Path(font_path).exists() else ImageFont.load_default()
def panel(bounds,width,height,selected,title):
    image=Image.new('RGB',(width,height),(239,241,244));draw=ImageDraw.Draw(image)
    lo,hi=np.array(bounds[0]),np.array(bounds[1]);extent=np.maximum(hi-lo,.01);scale=min((width-60)/extent[0],(height-100)/extent[1])
    center=(lo+hi)/2;xy=np.column_stack([(points[:,1]-center[0])*scale+width/2,(center[1]-points[:,2])*scale+height/2+20])
    face_points=xy[triangles];visible=(face_points[:,:,0].max(1)>=0)&(face_points[:,:,0].min(1)<width)&(face_points[:,:,1].max(1)>=50)&(face_points[:,:,1].min(1)<height)
    vectors=points[triangles];normal=np.cross(vectors[:,1]-vectors[:,0],vectors[:,2]-vectors[:,0]);normal/=np.maximum(np.linalg.norm(normal,axis=1)[:,None],1e-12)
    shade=np.clip(.45+.35*np.abs(normal[:,0]),0,1);depth=vectors[:,:,0].mean(1)
    for index in np.flatnonzero(visible)[np.argsort(depth[visible])]:
        grey=int(255*shade[index]);draw.polygon([tuple(q) for q in face_points[index]],fill=(grey,grey,grey))
    for component in selected:
        selected_faces=np.flatnonzero(visible&(face_labels==component)) if not a.previous_report else []
        for index in selected_faces:draw.polygon([tuple(q) for q in face_points[index]],fill=(225,71,32))
        ref=reference_points[reference_labels==component]
        bounds_points=np.column_stack([(ref[:,1]-center[0])*scale+width/2,(center[1]-ref[:,2])*scale+height/2+20]);minimum=bounds_points.min(0)-8;maximum=bounds_points.max(0)+8
        draw.rectangle([tuple(minimum),tuple(maximum)],outline=(185,35,20),width=2)
        draw.text((float(minimum[0]),float(minimum[1])-24),('prior ' if a.previous_report else '')+f'C{component:04}',font=font(20),fill=(145,25,15))
    draw.rectangle((0,0,width,48),fill=(30,39,52));draw.text((16,12),title,font=font(21),fill='white');return image
lo=points[:,[1,2]].min(0)-.12;hi=points[:,[1,2]].max(0)+.12
canvas=Image.new('RGB',(1600,1000),'white');canvas.paste(panel([lo,hi],1000,1000,a.component,'After cleanup: prior candidate locations' if a.previous_report else 'Rest mesh: selected disconnected candidates'),(0,0))
for index,component in enumerate(a.component[:2]):
    selected_points=reference_points[reference_labels==component][:,[1,2]];lo=selected_points.min(0)-.12;hi=selected_points.max(0)+.12
    title=f'Prior C{component:04} location after cleanup' if a.previous_report else f'C{component:04} close-up (shown through surface)'
    canvas.paste(panel([lo,hi],600,500,[component],title),(1000,index*500))
canvas.save(target/'Component-Review.png')
print(json.dumps(dict(output=str(target/'Component-Review.png'),components=a.component,method='CPU triangle painter; orthographic +X view; selected faces drawn last for identification, not occlusion proof.')))
