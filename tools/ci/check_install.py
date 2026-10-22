"""Exercise an installed wheel outside the source package, including native CLIs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import shutil
import sys
import tempfile
import zipfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--wheel', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--gui', action='store_true')
    parser.add_argument('--software-gl', action='store_true')
    parser.add_argument('--standalone-project', type=Path)
    args = parser.parse_args()
    import qupathxx as q
    from qupathxx import _native
    import numpy as np
    native = Path(_native.__file__).resolve()
    if 'site-packages' not in native.parts or 'src' in native.parts:
        raise RuntimeError('check must use an installed non-editable wheel')
    with zipfile.ZipFile(args.wheel) as wheel:
        member = next(n for n in wheel.namelist() if Path(n).name == native.name)
        if hashlib.sha256(wheel.read(member)).digest() != hashlib.sha256(native.read_bytes()).digest():
            raise RuntimeError('loaded native binary is not from the requested wheel')
        names = wheel.namelist()
        for suffix in ['licenses/geos/COPYING', 'licenses/sqlite/sqlite-LICENSE.txt', 'licenses/nlohmann-json/LICENSE.MIT']:
            if not any(name.endswith(suffix) for name in names):
                raise RuntimeError('missing runtime notice: '+suffix)
    assert q.has_gui == args.gui and q.has_geos and q.has_openslide and q.has_project and q.has_interchange
    with tempfile.TemporaryDirectory(prefix='qupathxx-install-') as directory:
        root = Path(directory)
        project = q.Project()
        doc = q.Document(q.Image(32, 24, [16]*32*24*3))
        obj = doc.add_rectangle(q.Rect(1, 2, 20, 16), 'Installed workflow')
        doc.set_calibration(.5, .25)
        transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
        request = q.AnalysisRequest(doc, obj, transform)
        result = q.analyze(request)
        doc.keep_analysis(request, result)
        tissue_request = q.TissueRequest(doc, obj, transform)
        doc.keep_tissue(tissue_request, q.detect_tissue(tissue_request))
        cell_request = q.CellRequest(doc, obj, transform)
        doc.keep_cells(cell_request, q.detect_cells(cell_request))
        image = project.add_document(doc)
        source = root/'workflow.qpxproj'
        project.save(str(source))
        if args.standalone_project:
            shutil.copy2(source, args.standalone_project)
        restored = q.Project.open(str(source)).document(image)
        assert restored.annotation(obj).name == 'Installed workflow'
        assert len(restored.analysis_records) == len(restored.tissue_records) == len(restored.cell_records) == 1
        q.save_measurements_csv(q.MeasurementTableRequest(restored, q.MeasurementTableKind.ROI), str(root/'means.csv'))
        q.save_geojson(restored, str(root/'objects.geojson'))
        region = restored.server.read_region(q.RegionRequest(0, 0, 0, 32, 24))
        assert region.numpy().shape == (24, 32, 4) and region.numpy().dtype == np.uint8
        suffix = '.exe' if os.name == 'nt' else ''
        cli = native.parent/('qupathxx_batch'+suffix)
        batch = subprocess.run([str(cli), str(source), str(root/'batch'), '--kind', 'stains'],
                               capture_output=True, timeout=60)
        if batch.returncode:
            raise RuntimeError(batch.stderr.decode('utf8', errors='replace'))
        checkpoints = list((root/'batch').rglob('*.qpxproj'))
        assert len(checkpoints) == 1 and len(q.Project.open(str(checkpoints[0])).document(image).analysis_records) == 2
        renderer = ''
        if args.gui:
            report = q.show(q.Project.open(str(source)), frame_limit=20)
            renderer = report.gl_renderer
            if args.software_gl and 'llvmpipe' not in renderer.lower():
                raise RuntimeError('GUI check did not use the required software renderer: '+renderer)
        report = dict(schema='qpx.installed-workflow/1', python=sys.version, executable=sys.executable,
                      native=str(native), native_sha256=hashlib.sha256(native.read_bytes()).hexdigest(),
                      source_tree_sha256=q.build_environment().source_tree_sha256,
                      gui=q.has_gui, geos=q.has_geos, openslide=q.has_openslide, gl_renderer=renderer,
                      roi_samples=result.samples, native_batch_checkpoints=len(checkpoints))
    args.output.write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
