"""Independent RGB phantoms for the pinned cell-workflow comparison."""
from pathlib import Path
import json


ROOT = Path(__file__).resolve().parents[1]
CONTRACT = ROOT / 'tests/fixtures/qupath-cells-0.7.0/contract.json'


def read_contract():
    return json.loads(CONTRACT.read_text(encoding='utf-8'))


def pixels(contract, case):
    import numpy as np
    y, x = np.mgrid[:contract['height'], :contract['width']]
    x, y = x+.5, y+.5
    rgb = np.empty((contract['height'], contract['width'], 3), dtype='float64')
    rgb[:] = [245,244,242]
    if 'noise_seed' in case:
        rgb += ((x/contract['width'])*-10)[:, :, None]
        rgb += np.random.default_rng(case['noise_seed']).normal(0,1.25,rgb.shape)
    # Cytoplasm is deliberately below the declared OD-sum detection threshold.
    for cx, cy, rx, ry in case['nuclei']:
        mask = ((x-cx)/(rx+5))**2+((y-cy)/(ry+5))**2 <= 1
        rgb[mask] = [225,200,210]
    for cx, cy, rx, ry in case['nuclei']:
        mask = ((x-cx)/rx)**2+((y-cy)/ry)**2 <= 1
        rgb[mask] = [75,55,120]
    return np.clip(np.rint(rgb), 0, 255).astype('uint8')


def generate(folder, contract=None):
    from PIL import Image
    contract = contract or read_contract()
    folder = Path(folder)
    folder.mkdir(parents=True, exist_ok=False)
    for case in contract['cases']:
        destination = folder/case['name']
        destination.mkdir()
        Image.fromarray(pixels(contract,case)).save(destination/'input.png')
    (folder/'contract.json').write_text(json.dumps(contract,indent=2)+'\n',encoding='utf-8')
    return folder
