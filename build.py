"""Reproducible local Windows x64 build using official Zig 0.15.2 and OBS 32.2.2 headers."""
from pathlib import Path
import base64, json, os, struct, subprocess, shutil
from generate_translations import generate

ROOT = Path(__file__).resolve().parent
BUILD = ROOT / 'build'
BUILD.mkdir(exist_ok=True)
generate(ROOT)
# Keep the author logo inside the DLL, including for portable installations.
logo = base64.b64encode((ROOT/'assets/DjEmiMas.png').read_bytes()).decode('ascii')
logo_html = '<img width="80" height="80" alt="DjEmiMas" src="data:image/png;base64,' + logo + '">'
(BUILD/'branding.h').write_text('static const char sketch_logo_html[] =\n' + '\n'.join(json.dumps(logo_html[i:i+1000]) for i in range(0, len(logo_html), 1000)) + ';\n')
ZIG = ROOT / 'build-tools/zig-x86_64-windows-0.15.2/zig.exe'
SDK = ROOT / 'build-tools/obs-studio-32.2.2'
OBS = Path(os.environ.get('OBS_INSTALL_DIR', r'C:\Program Files\obs-studio'))
os.environ['ZIG_GLOBAL_CACHE_DIR'] = str(BUILD / 'zig-cache')
os.environ['ZIG_LOCAL_CACHE_DIR'] = str(BUILD / 'zig-local')

# Build an import library from the exports of the installed, official obs.dll.
# This reads the PE export table; it does not execute the DLL.
raw = (OBS / 'bin/64bit/obs.dll').read_bytes()
u16 = lambda p: struct.unpack_from('<H', raw, p)[0]
u32 = lambda p: struct.unpack_from('<I', raw, p)[0]
pe = u32(0x3c)
assert raw[pe:pe+4] == b'PE\0\0' and u16(pe+4) == 0x8664
opt = pe + 24
sections = opt + u16(pe+20)
def offset(rva):
    for i in range(u16(pe+6)):
        s = sections + i*40
        va, size = u32(s+12), max(u32(s+8), u32(s+16))
        if va <= rva < va+size:
            return u32(s+20) + rva-va
    raise ValueError('RVA not found')
exp = offset(u32(opt+112))
names = offset(u32(exp+32))
exports = []
for i in range(u32(exp+24)):
    p = offset(u32(names+4*i))
    exports.append(raw[p:raw.index(b'\0', p)].decode('ascii'))
(BUILD/'obs.def').write_text('LIBRARY obs.dll\nEXPORTS\n'+'\n'.join(exports)+'\n')
subprocess.run([str(ZIG), 'dlltool', '-m', 'i386:x86-64', '-d', str(BUILD/'obs.def'), '-l', str(BUILD/'obs.lib')], check=True)
(BUILD/'obsconfig.h').write_text('#pragma once\n#define OBS_RELEASE_CANDIDATE 0\n#define OBS_BETA 0\n')
shader = (ROOT/'plugin/obs-sketch.effect').read_text(encoding='utf-8')
(BUILD/'sketch-shader.h').write_text('static const char sketch_shader[] =\n'+ '\n'.join(json.dumps(line+'\n') for line in shader.splitlines()) + ';\n')
common = [str(ZIG), 'cc', '-target', 'x86_64-windows-gnu', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-I'+str(SDK/'libobs'), '-I'+str(BUILD)]
out = ROOT/'dist/obs-sketch/bin/64bit'
out.mkdir(parents=True, exist_ok=True)
subprocess.run(common + ['-shared', str(ROOT/'src/obs-sketch.c'), str(BUILD/'obs.lib'), '-o', str(BUILD/'obs-sketch.dll')], check=True)
shutil.copy2(BUILD/'obs-sketch.dll', out/'obs-sketch.dll')
if (ROOT/'tests/gpu-test.c').exists():
    subprocess.run(common + [str(ROOT/'tests/gpu-test.c'), str(BUILD/'obs.lib'), '-o', str(BUILD/'gpu-test.exe')], check=True)
if (ROOT/'tests/motion-test.c').exists():
    subprocess.run(common + [str(ROOT/'tests/motion-test.c'), '-o', str(BUILD/'motion-test.exe')], check=True)
if (ROOT/'tests/locale-test.c').exists():
    subprocess.run(common + [str(ROOT/'tests/locale-test.c'), '-o', str(BUILD/'locale-test.exe')], check=True)
print('Native filter built:', out/'obs-sketch.dll')
