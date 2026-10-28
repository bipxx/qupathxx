from pathlib import Path
import hashlib
import json
from PIL import Image

root=Path(__file__).resolve().parents[2];out=root/'build/nominal-cell-frontends-294'
captures=sorted((out/'desktop-temp').rglob('nominal-desktop.ppm'));assert len(captures)==2
proof=[]
for index,source in enumerate(captures):
    destination=out/('nominal-desktop-'+str(index)+'.png');assert not destination.exists()
    with Image.open(source) as original:
        original.save(destination)
        with Image.open(destination) as encoded:assert original.size==encoded.size and original.tobytes()==encoded.tobytes()
        proof.append(dict(source=str(source),png=destination.name,width=original.width,height=original.height,
            source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),png_sha256=hashlib.sha256(destination.read_bytes()).hexdigest(),lossless=True))
(out/'desktop-capture-proof.json').write_text(json.dumps(dict(captures=proof,raw_ppm_retained_locally=True),indent=2)+'\n',encoding='utf-8')
print('Two actual desktop captures converted losslessly for inspection',flush=True)
