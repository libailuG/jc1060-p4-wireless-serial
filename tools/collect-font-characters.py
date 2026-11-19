"""GB2312 level-one Hanzi plus CJK characters present in application sources."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
common=set()
for lead in range(0xb0,0xd8):
    for trail in range(0xa1,0xff):
        try: common.update(bytes([lead,trail]).decode('gb2312'))
        except UnicodeDecodeError: pass
assert len(common)==3755
required=set()
for p in (root/'main').rglob('*'):
    if p.suffix not in ('.c','.h') or 'fonts' in p.parts: continue
    for c in p.read_text(encoding='utf-8-sig'):
        if 0x3400<=ord(c)<=0x9fff or 0xf900<=ord(c)<=0xfaff: required.add(c)
# Additional user-required glyphs can be added to this UTF-8 file.
extra=root/'tools/font-extra-characters.txt'
if extra.exists(): required.update(c for c in extra.read_text(encoding='utf-8-sig') if not c.isspace())
chars=''.join(sorted(common|required))
(root/'tools/font-characters.txt').write_text(chars,encoding='utf-8')
(root/'tools/font-required-characters.txt').write_text(''.join(sorted(required)),encoding='utf-8')
print(f'Common: {len(common)}, application: {len(required)}, union: {len(chars)}')
