"""Render the unpainted chest in FOTOS through the real OBS filter DLL.

The lower, hand-painted photo is a style reference, never filter input.
Requires Pillow, OBS and a built gpu-test.exe. No image generation is used.
"""
from pathlib import Path
import os
import subprocess
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'tests/artifacts/reference'
OUT.mkdir(parents=True, exist_ok=True)
reference = Image.open(ROOT / 'FOTOS/474212826_915991793852405_8164925281253054869_n.jpg')
source = reference.crop((0, 0, reference.width, 785)).convert('RGBA')
(OUT / 'input.rgba').write_bytes(source.tobytes())
obs = Path(os.environ.get('OBS_INSTALL_DIR', r'C:\Program Files\obs-studio'))
env = os.environ.copy()
env['PATH'] = str(obs / 'bin/64bit') + os.pathsep + env.get('PATH', '')

def render(dll, name):
    destination = OUT / name
    destination.mkdir(exist_ok=True)
    run = subprocess.run([str(ROOT/'build/gpu-test.exe'), str(obs), str(dll),
                          str(OUT/'input.rgba'), str(source.width), str(source.height),
                          str(destination)], cwd=ROOT, env=env, capture_output=True)
    (destination/'gpu-test.log').write_bytes(run.stdout + run.stderr)
    if run.returncode:
        raise RuntimeError(f'GPU render failed: {destination}/gpu-test.log')
    return destination

new = render(ROOT/'dist/obs-sketch/bin/64bit/obs-sketch.dll', 'new')

def load(path):
    return Image.frombytes('RGBA', source.size, path.read_bytes()).convert('RGB')

panels = [
    ('Cómic a color', load(new/'mode-0.rgba')),
    ('Boceto blanco y negro', load(new/'mode-1.rgba')),
    ('Tinta sobre color original', load(new/'mode-2.rgba')),
    ('Pintura blanca y rotulador', load(new/'mode-3.rgba')),
]
width = 1200
height = round(source.height * width / source.width)
canvas = Image.new('RGB', (width*2, (height+52)*2+58), '#202026')
draw = ImageDraw.Draw(canvas)
font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 24)
small = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 19)
for i, (label, im) in enumerate(panels):
    x=(i%2)*width; y=(i//2)*(height+52)
    draw.text((x+20,y+10), label, fill='white', font=font)
    canvas.paste(im.resize((width,height), Image.Resampling.LANCZOS), (x,y+52))
draw.text((20,canvas.height-44), 'OBS Sketch 1.3 · Misma entrada y mismos ajustes: solo cambia Acabado · Resultado de la DLL real', fill='#dddddd', font=small)
canvas.save(OUT/'cuatro-acabados.png')
load(new/'mode-3.rgba').save(OUT/'resultado-baul.png')
if (new/'color-reveal.rgba').exists():
    color_canvas=Image.new('RGB',(width,(height+52)*2),'#202026')
    color_draw=ImageDraw.Draw(color_canvas)
    for i,(name,label) in enumerate([('mode-3','Revelar color original: 0 %'),('color-reveal','Revelar color original: 100 % (se conserva la tinta)')]):
        y=i*(height+52)
        color_draw.text((20,y+10),label,fill='white',font=font)
        color_canvas.paste(load(new/(name+'.rgba')).resize((width,height),Image.Resampling.LANCZOS),(0,y+52))
    color_canvas.save(OUT/'revelar-color.png')
print(OUT/'cuatro-acabados.png')
