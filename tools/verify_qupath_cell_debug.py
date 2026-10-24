"""Verify the pinned plugin's own debug stages against its ordinary capture."""
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/qupath-ndpi-cells-0.7.0'


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def load(name,file):
    spec=importlib.util.spec_from_file_location(name,ROOT/'tools'/file)
    value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value


def verify(folder=FIXTURE/'debug'):
    import numpy as np
    from PIL import Image
    comparison=load('qpx_debug_capture','compare_qupath_ndpi_cells.py')
    matcher=load('qpx_debug_metrics','label_comparison.py')
    contract,reference=comparison.verify_capture(FIXTURE/'capture')
    manifest=json.loads((folder/'manifest.json').read_text())
    if (manifest['schema']!='qpx.qupath-cell-debug/1' or
            manifest['capture_manifest_sha256']!=sha(FIXTURE/'capture/capture-manifest.json') or
            manifest['base_generator_sha256']!=sha(ROOT/'tools/upstream/qupath_ndpi_cells.groovy') or
            manifest['debug_generator_sha256']!=sha(ROOT/'tools/upstream/qupath_cell_debug_probe.groovy') or
            set(manifest['files'])!={'debug-stack.tif','debug-stack.json','driver.log'}):
        raise ValueError('debug capture identity differs')
    for name,identity in manifest['files'].items():
        path=folder/name
        if path.stat().st_size!=identity['bytes'] or sha(path)!=identity['sha256']:
            raise ValueError('debug capture bytes changed')
    metadata=json.loads((folder/'debug-stack.json').read_text())
    if metadata!=dict(width=256,height=256,slices=['Input image','Laplacian of Gaussian filtered','Watershed labels','Binary','Labeled ROIs']):
        raise ValueError('unexpected debug stages')
    with Image.open(folder/'debug-stack.tif') as image:
        if image.n_frames!=5:raise ValueError('unexpected debug frame count')
        stages=[]
        for i in range(5):
            image.seek(i);stage=np.asarray(image).copy()
            if stage.shape!=(256,256) or not np.all(np.isfinite(stage)):
                raise ValueError('invalid debug stage pixels')
            stages.append(stage)
    with Image.open(FIXTURE/'capture/input.png') as image:rgb=np.asarray(image.convert('RGB'))
    expected=(-np.log10(np.maximum(rgb.astype(float),1)/255.)).sum(axis=2)
    error=float(np.max(np.abs(stages[0]-expected)))
    if error>1e-6:raise ValueError('debug input optical density differs')
    if np.any(stages[-1]<0) or np.any(stages[-1]!=np.floor(stages[-1])):
        raise ValueError('debug labels are not nonnegative integers')
    labels=comparison.reference_labels(FIXTURE/'capture',contract,reference)['nucleus']
    metrics=matcher.metrics(labels,stages[-1].astype(np.uint32),contract['comparison'])
    if (metrics['reference_count']!=506 or metrics['native_count']!=506 or
            metrics['foreground_iou']!=1 or metrics['unmatched_reference'] or metrics['unmatched_native'] or
            any(m['iou']!=1 or m['centroid_error_px']!=0 for m in metrics['matches'])):
        raise ValueError('debug nuclei differ from ordinary plugin output')
    return dict(optical_density_max_abs_error=error,nuclei=506,nucleus_masks_exact=True,
                stages=[dict(name=name,positive_pixels=int(np.count_nonzero(stage>0)))
                        for name,stage in zip(metadata['slices'],stages)])


if __name__=='__main__':print(json.dumps(verify(),indent=2))
