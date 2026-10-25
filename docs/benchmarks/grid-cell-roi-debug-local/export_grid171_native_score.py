from pathlib import Path
import json
from PIL import Image
import numpy as np
import qupathxx as q
r=Path(__file__).resolve().parent/'scientific-grid-159';out=r.parents[1].parent/'build/grid171-roi-debug'
contract=json.loads((r/'tests/fixtures/qupath-cells-0.7.0/contract.json').read_text(encoding='utf-8'))
with Image.open(out/'roi_cut_hole/input.png') as image:rgb=np.asarray(image.convert('RGB'))
server=q.dense_image_server(q.Image.from_numpy(rgb));grid=q.ScientificGrid(256,192,q.ScientificGridAxis(32,192,192),q.ScientificGridAxis(32,128,128))
source=q.read_scientific_rgb(server,grid);channels=q.scientific_optical_densities(source,q.StainTransform(contract['vectors'],contract['white'])).numpy()
# Preserve the C++ expression's left-associative addition explicitly.
score=(channels[:,:,0]+channels[:,:,1])+channels[:,:,2]
(out/'native-input-od-f64le.bin').write_bytes(score.astype('<f8').tobytes())
