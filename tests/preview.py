from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw
p = Path(__file__).parent / 'artifacts'
a = np.frombuffer((p/'original.rgba').read_bytes(),np.uint8).reshape(320,512,4)
b = np.frombuffer((p/'bypass.rgba').read_bytes(),np.uint8).reshape(a.shape)
d = np.abs(a.astype(int)-b.astype(int))
print('Bypass maximum difference by channel:',d.max(axis=(0,1)), 'changed pixels:',(d>0).sum(axis=(0,1)))
print('Sample differing pixels:',a[d.max(axis=2)>0][:8],b[d.max(axis=2)>0][:8])
canvas = Image.new('RGB',(1024,1080),(25,25,28))
draw = ImageDraw.Draw(canvas)
for k,(name,label) in enumerate(zip(['original','mode-3','mode-0','mode-1','mode-2'],['Original','Pintura blanca y rotulador','Comic a color','Boceto blanco y negro','Tinta sobre color original'])):
    r=np.frombuffer((p/(name+'.rgba')).read_bytes(),np.uint8).reshape(320,512,4).astype(float)
    alpha=r[:,:,3:4]/255
    yy,xx=np.indices((320,512))
    bg=np.where(((xx//16+yy//16)%2)[...,None],80,55)
    rgb=np.clip(r[:,:,:3]+bg*(1-alpha),0,255).astype(np.uint8)
    x=(k%2)*512; y=(k//2)*360
    canvas.paste(Image.fromarray(rgb),(x,y+30))
    draw.text((x+12,y+8),label,fill='white')
canvas.save(p/'comparison.png')
