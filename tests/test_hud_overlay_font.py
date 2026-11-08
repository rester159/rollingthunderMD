#!/usr/bin/env python3
"""Every valid HUD glyph retains its shape and palette index in all ten areas.
Golden hashes come from the original colored atlases in revision 1460416.
Blank pixels remain transparent; this does not certify hardware sprite limits.
"""
import hashlib,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
EXPECTED={'hud00_data.c': '5045cba88421f71932f24d8b53b4b5a8bb30fe6c7beac63141686928dc853bb9', 'hud01_data.c': 'ced3173f4ccc8066c34a89bdcec3bccb68eef7e930e1ccd888b3043cea7f3d72', 'hud02_data.c': '84b6d8051058fe97e0c57ae49a942dcdb52a24ee47bc7c8b4c6c61b862672e7c', 'hud03_data.c': '8ce8194d09e797723bda2eec71b7bbf978796e8a8badea01b678e68a02f1da8e', 'hud04_data.c': 'fbc80b093a0db86f8251741fd756091a96fad7a22ffa42984419da970d745b8a', 'hud10_data.c': '96d41d83568a7ff52fa2352b82b2fa8414e7861698fa6dadac6cb5bf5b7c1304', 'hud11_data.c': 'ced3173f4ccc8066c34a89bdcec3bccb68eef7e930e1ccd888b3043cea7f3d72', 'hud12_data.c': 'a5cc764e416bae087ffdcb8ffcb83e219ea9be750b3c9ce29813fd1b8ff92043', 'hud13_data.c': 'adbb71478fb014186c79fbca1f6494333a73a5c4ae2166326c69f026b7ce0510', 'hud14_data.c': 'ced3173f4ccc8066c34a89bdcec3bccb68eef7e930e1ccd888b3043cea7f3d72'}
def array(text,name):
    body=re.search(r'\b'+name+r'\[.*?=(\{.*?\});',text,re.S).group(1)
    return [int(v,0) for v in re.findall(r'0x[0-9a-f]+|\d+',body)]
patterns=array((ROOT/'src/hud_font.c').read_text(),'rtHudMonochromePatterns')
assert len(patterns)==696
assert all(((word>>shift)&15) in (0,1) for word in patterns for shift in range(0,32,4))
count=0
for name,digest in EXPECTED.items():
    text=(ROOT/'src'/name).read_text();attrs=array(text,'attributes');inks=array(text,'inks');pixels=bytearray()
    assert len(attrs)==5*256 and len(inks)==5
    for i,a in enumerate(attrs):
        if a==65535:continue
        assert not a&0x1800, 'Unexpected glyph flip'
        count+=1
        for word in patterns[(a&2047)*8:(a&2047)*8+8]:
            pixels.extend(inks[i//256] if (word>>shift)&15 else 0 for shift in range(28,-1,-4))
    assert hashlib.sha256(pixels).hexdigest()==digest,name
print(f'{count} HUD glyph variants across all ten areas retain every colored pixel; backgrounds are transparent.')
