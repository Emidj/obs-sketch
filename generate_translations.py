"""Validate the shared catalogue and embed translations in the DLL and installer."""
from pathlib import Path
import json

def generate(root):
    data=json.loads((root/'locale/strings.json').read_text(encoding='utf-8'))
    assert data['languages']==['en','es','it','fr','de'], 'Unexpected language order'
    rows=data['strings']
    for key,values in rows.items():
        if len(values)!=5 or any(not isinstance(v,str) or not v.strip() or '\0' in v for v in values):
            raise ValueError(f'Incomplete translation: {key}')
    build=root/'build'
    build.mkdir(exist_ok=True)
    quote=lambda value: json.dumps(value,ensure_ascii=True)
    c=['/* Generated from locale/strings.json. Do not edit. */',
       'struct sketch_translation { const char *key; const char *text[5]; };',
       'static const struct sketch_translation sketch_translations[] = {']
    for key,values in rows.items():
        if not key.startswith('setup.'):
            c.append('    {'+quote(key)+', {'+', '.join(map(quote,values))+'}},')
    c.append('};')
    (build/'translations.h').write_text('\n'.join(c)+'\n',encoding='utf-8')
    cs=['// Generated from locale/strings.json. Do not edit.',
        'using System.Collections.Generic;', 'using System.Runtime.InteropServices;',
        'internal static class LocaleText {',
        '[DllImport("kernel32.dll")] static extern ushort GetUserDefaultUILanguage();',
        'internal static int LanguageForId(int id) { switch (id & 0x3ff) {',
        'case 0x0a: return 1; case 0x10: return 2; case 0x0c: return 3; case 0x07: return 4; default: return 0; } }',
        'internal static int Language { get { return LanguageForId(GetUserDefaultUILanguage()); } }',
        'internal static readonly Dictionary<string,string[]> Entries = new Dictionary<string,string[]> {']
    for key,values in rows.items():
        if key.startswith('setup.'):
            cs.append('{'+quote(key)+',new string[]{'+','.join(map(quote,values))+'}},')
    cs+=['};',
         'internal static string Text(string key) { return Entries[key][Language]; }', '}']
    (build/'LocaleText.cs').write_text('\n'.join(cs)+'\n',encoding='utf-8')

if __name__=='__main__':
    generate(Path(__file__).resolve().parent)
