"""Actual DLL comparison, using a known simulated camera pan of the raw chest."""
from pathlib import Path
import os
import subprocess
from PIL import Image, ImageDraw, ImageFont

root=Path(__file__).resolve().parents[1]
out=root/'tests/artifacts/motion'
out.mkdir(parents=True,exist_ok=True)
photo=Image.open(root/'FOTOS/474212826_915991793852405_8164925281253054869_n.jpg')
source=photo.crop((0,0,photo.width,785)).convert('RGBA')
source=source.resize((960,460),Image.Resampling.LANCZOS)
(out/'input.rgba').write_bytes(source.tobytes())
obs=Path(os.environ.get('OBS_INSTALL_DIR',r'C:\Program Files\obs-studio'))
env=os.environ.copy()
env['PATH']=str(obs/'bin/64bit')+os.pathsep+env.get('PATH','')
run=subprocess.run([str(root/'build/gpu-test.exe'),str(obs),str(root/'dist/obs-sketch/bin/64bit/obs-sketch.dll'),
                    str(out/'input.rgba'),'960','460',str(out),'--motion'],env=env,cwd=root,capture_output=True)
(out/'gpu-motion.log').write_bytes(run.stdout+run.stderr)
print(run.stdout.decode(errors='replace')[-800:])
if run.returncode:
    raise RuntimeError('Motion GPU test failed; see gpu-motion.log')
font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',22)
frames=[]
for step in range(21):
    canvas=Image.new('RGB',(960,1016),'#202026')
    draw=ImageDraw.Draw(canvas)
    for follow,label in enumerate(['Trama fija (seguimiento desactivado)','Trama que sigue la cámara (seguimiento activado)']):
        y=follow*500
        draw.text((16,y+8),label,font=font,fill='white')
        im=Image.frombytes('RGBA',source.size,(out/f'motion-{follow}-{step:02}.rgba').read_bytes())
        canvas.paste(im,(0,y+40),im)
    if step==20: canvas.save(out/'comparacion-movimiento.png')
    frames.append(canvas)
frames[0].save(out/'seguimiento-camara.gif',save_all=True,append_images=frames[1:],duration=[600]+[80]*19+[600],loop=0)
print(out/'seguimiento-camara.gif')
