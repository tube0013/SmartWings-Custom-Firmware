#!/usr/bin/env python3
"""Verify hashes, addressed image checksums and first-install region contents."""
import hashlib, json, struct
from pathlib import Path
root=Path(__file__).resolve().parents[1];release=root/'firmware/Q17'
def load(path):
    result={};base=0
    for line in path.read_text().splitlines():
        if line.startswith(':'):
            row=bytes.fromhex(line[1:]);assert sum(row)&255==0, path
            assert len(row)==row[0]+5,path
            n=row[0];addr=int.from_bytes(row[1:3],'big');kind=row[3];data=row[4:4+n]
            if kind==4:base=int.from_bytes(data,'big')<<16
            elif kind==2:base=int.from_bytes(data,'big')<<4
            elif kind==0:
                for i,b in enumerate(data):
                    at=base+addr+i;assert at not in result or result[at]==b,path;result[at]=b
        elif line.startswith('S'):
            row=bytes.fromhex(line[2:]);assert sum(row)&255==255,path;assert len(row)==row[0]+1,path
            kind=int(line[1]);width={1:2,2:3,3:4}.get(kind)
            if width:
                at=int.from_bytes(row[1:1+width],'big')
                for i,b in enumerate(row[1+width:-1]):
                    assert at+i not in result or result[at+i]==b,path;result[at+i]=b
    assert result,path
    return result
for ln in (release/'SHA256SUMS').read_text().splitlines():
    digest,name=ln.split('  ',1);assert hashlib.sha256((release/name).read_bytes()).hexdigest()==digest,name
app=load(release/'smartwings-Q17-app.s37');boot=load(release/'smartwings-bootloader.s37')
assert app==load(release/'smartwings-Q17-app.hex');assert boot==load(release/'smartwings-bootloader.hex')
assert all(0x4000<=a<0x5c000 for a in app);assert all(0<=a<0x4000 for a in boot)
for name,m,start in [('app',app,0x4000),('bootloader',boot,0)]:
    data=(release/('smartwings-Q17-app.bin' if name=='app' else 'smartwings-bootloader.bin')).read_bytes()
    assert all(data[at-start]==value for at,value in m.items()),name
    assert len(data)==max(m)-start+1,name
combined=load(release/'smartwings-Q17-combined.s37')
assert combined==load(release/'smartwings-Q17-initial.hex')
assert combined==dict(boot,**{})|app|{i:255 for i in range(0xb6000,0xbe000)}
assert load(release/'smartwings-Q17-update.s37')==boot|app
ota=(release/'smartwings-Q17-app.ota').read_bytes();h=struct.unpack_from('<IHHHHHIH32sI',ota)
assert h[0]==0x0beef11e and h[4:7]==(0xfffe,0x6aad,17) and h[-1]==len(ota)
tag,n=struct.unpack_from('<HI',ota,h[2]);assert tag==0 and n==len(ota)-h[2]-6
assert ota[h[2]+6:]==(release/'smartwings-Q17-app.gbl').read_bytes()
assert struct.unpack_from('<HH',ota,56)==(1,1)
print(json.dumps({'hashes':'PASS','image_checksums':'PASS','combined_matches_initial_hex':'PASS','factory_regions_absent':'PASS','app_bytes':len(app),'bootloader_bytes':len(boot),'ota_bytes':len(ota)},indent=2))
