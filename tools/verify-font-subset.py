"""Verify subset coverage and unchanged glyph metrics/bitmap against a full font."""
import re,sys
from pathlib import Path
def read_font(path):
    s=Path(path).read_text(encoding='utf-8-sig')
    def arr(name):
        m=re.search(r'\b'+name+r'\[\]\s*=\s*\{(.*?)\};',s,re.S)
        body=re.sub(r'/\*.*?\*/','',m[1],flags=re.S)
        return [int(x,0) for x in re.findall(r'0x[0-9a-fA-F]+|\b\d+\b',body)]
    bitmap=bytes(arr('glyph_bitmap'))
    ds=re.search(r'glyph_dsc\[\]\s*=\s*\{(.*?)\n\};',s,re.S)[1]
    glyphs=[dict((k,int(v)) for k,v in re.findall(r'\.(\w+)\s*=\s*(-?\d+)',entry)) for entry in re.findall(r'\{([^{}]*)\}',ds)]
    cm=re.search(r'cmaps\[\]\s*=\s*\{(.*?)\n\};',s,re.S)[1]
    mapping={}
    for entry in re.findall(r'\{([^{}]*)\}',cm):
        fields=dict(re.findall(r'\.(\w+)\s*=\s*(\w+)',entry))
        start=int(fields['range_start']); gid=int(fields['glyph_id_start']);typ=fields['type']
        offsets=arr(fields['unicode_list']) if fields['unicode_list']!='NULL' else list(range(int(fields['range_length'])))
        gids=arr(fields['glyph_id_ofs_list']) if fields['glyph_id_ofs_list']!='NULL' else None
        for j,off in enumerate(offsets):
            if gids and 'FORMAT0_FULL' in typ and j>0 and gids[j]==0: continue
            mapping[start+off]=gid+(gids[j] if gids else j)
    return bitmap,glyphs,mapping
old,old_g,old_m=read_font(sys.argv[1]);new,new_g,new_m=read_font(sys.argv[2])
required=Path(sys.argv[3]).read_text(encoding='utf-8')
missing=[c for c in required if ord(c) not in new_m]
assert not missing,repr(missing)
for cp,gid in new_m.items():
    a=old_g[old_m[cp]];b=new_g[gid]
    assert all(a[k]==b[k] for k in ('adv_w','box_w','box_h','ofs_x','ofs_y')),hex(cp)
    length=(b['box_w']*b['box_h']+1)//2
    assert old[a['bitmap_index']:a['bitmap_index']+length]==new[b['bitmap_index']:b['bitmap_index']+length],hex(cp)
print(f'PASS: {len(required)} required Hanzi covered; {len(new_m)} retained glyphs match original metrics and bitmap.')
print(f'Bitmap bytes: {len(old)} -> {len(new)}; glyphs: {len(old_g)} -> {len(new_g)}')
