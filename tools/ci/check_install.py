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
    parser.add_argument('--openslide-version')
    parser.add_argument('--openslide-runtime-sha256')
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
        required = ['libtiff/LICENSE.md', 'tiff-zlib/LICENSE', 'tinyxml2/LICENSE.txt', 'nanobind/LICENSE', 'robin-map/LICENSE', 'opencv/LICENSE',
                    'opencv/softfloat-notice.txt', 'opencv/zlib-notice.txt', 'opencv/SOURCE-NOTICES.txt']
        if args.gui:
            required += ['glfw/LICENSE.md', 'imgui/LICENSE.txt', 'imgui/ProggyClean-LICENSE.txt',
                         'imgui/Khronos-LICENSE.txt', 'imgui/Khronos-NOTICE.txt', 'imgui/imgl3w-LICENSE.txt']
            required += ['imgui/'+h+'-LICENSE.txt' for h in ['imstb_rectpack.h', 'imstb_textedit.h', 'imstb_truetype.h']]
        for relative in required:
            member = 'qupathxx/licenses/'+relative
            if member not in names or len(wheel.read(member)) < 100:
                raise RuntimeError('missing dependency notice: '+relative)
    assert q.has_gui == args.gui and q.has_geos and q.has_openslide and q.has_project and q.has_interchange
    assert q.has_classifier_training and q.has_tiff
    reader_version = q.build_environment().openslide_version
    if args.openslide_version and reader_version != args.openslide_version:
        raise RuntimeError('loaded OpenSlide version differs from the verified SDK: '+reader_version)
    reader_runtime_sha256 = None
    if args.openslide_runtime_sha256:
        runtime = native.parent/'libopenslide.so.1'
        reader_runtime_sha256 = hashlib.sha256(runtime.read_bytes()).hexdigest()
        with zipfile.ZipFile(args.wheel) as wheel:
            packaged_sha = hashlib.sha256(wheel.read('qupathxx/libopenslide.so.1')).hexdigest()
            assert wheel.read('qupathxx/licenses/openslide-sdk/OpenSlide/COPYING.LESSER')
        if reader_runtime_sha256 != args.openslide_runtime_sha256 or packaged_sha != reader_runtime_sha256:
            raise RuntimeError('bundled OpenSlide runtime differs from the verified SDK')
        mapped = {Path(line.split(maxsplit=5)[5]).resolve() for line in Path('/proc/self/maps').read_text(encoding='utf8').splitlines()
                  if len(line.split(maxsplit=5)) == 6 and '/libopenslide.so' in line.split(maxsplit=5)[5]}
        if mapped != {runtime.resolve()}:
            raise RuntimeError('native module did not load its bundled OpenSlide runtime: '+repr(mapped))
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
        # Quantitative records use original samples and survive a project round trip.
        import tifffile
        raw_values=np.arange(2*16*24,dtype='uint16').reshape(2,16,24)
        raw_source=root/'channels.ome.tif'
        tifffile.imwrite(raw_source,raw_values,ome=True,photometric='minisblack',byteorder='>',compression='deflate',metadata={'axes':'CYX'})
        raw_project=q.Project();raw_image=raw_project.add_tiff_image(str(raw_source));raw_doc=raw_project.document(raw_image)
        raw_object=raw_doc.add_rectangle(q.Rect(0,0,24,16),'Native raw')
        raw_region=q.RawRegionRequest();raw_region.region=q.RegionRequest(0,0,0,24,16);raw_region.channels=2
        raw_request=q.RawAnalysisRequest(raw_doc,raw_region,raw_object);raw_result=q.analyze_raw(raw_request)
        np.testing.assert_allclose(raw_result.statistics.means,raw_values.mean(axis=(1,2)),rtol=1e-12)
        assert q.raw_analysis_input_digest(raw_request).input_sha256==raw_result.input_sha256
        raw_record=raw_doc.keep_raw_analysis(raw_request,raw_result);raw_path=root/'raw.qpxproj';raw_project.save(str(raw_path))
        raw_loaded=q.Project.open(str(raw_path)).document(raw_image);assert raw_loaded.raw_analysis_status(raw_record)=='unverified'
        raw_replay=q.RawAnalysisRequest.replay(raw_loaded,raw_record);raw_revision=raw_loaded.revision
        raw_verified=raw_loaded.verify_raw_analysis(raw_record,raw_replay,q.analyze_raw(raw_replay))
        assert raw_verified and raw_loaded.revision==raw_revision
        q.save_raw_analysis_csv(q.RawTableRequest(raw_loaded,[raw_record],True),str(root/'raw.csv'))
        raw_batch_reused=False
        for attempt in range(2):
            raw_batch=subprocess.run([str(cli),str(raw_path),str(root/'raw-batch'),'--kind','raw','--raw-channels','2'],capture_output=True,timeout=60)
            if raw_batch.returncode:raise RuntimeError(raw_batch.stderr.decode('utf8',errors='replace'))
            summary=next(json.loads(line) for line in raw_batch.stdout.decode('utf8').splitlines() if json.loads(line)['schema']=='qpx.project-batch/1')
            assert summary['saved']==(attempt==0) and summary['reused']==(attempt==1)
            raw_batch_reused=attempt==1
        raw_loaded.server.close();raw_doc.server.close()
        renderer = ''
        if args.gui:
            requested = q.GuiOptions()
            requested.docked_workspace = True
            requested.ui_scale = 1.25
            requested.layout_path = str(root/'preferences'/'workspace.ini')
            requested.settle_tiles = True
            report = q.show(q.Project.open(str(source)), frame_limit=20, options=requested)
            renderer = report.gl_renderer
            if not report.docked_workspace or not report.layout_saved or report.layout_error:
                raise RuntimeError('installed docked workspace did not save its separate layout preferences')
            assert report.browser_bounds.x < report.canvas_bounds.x < report.display_bounds.x
            assert report.canvas_bounds.y < report.measurement_bounds.y
            assert report.ui_overflow_items == 0 and report.ui_max_overflow <= .75, (report.ui_overflow_label, report.ui_max_overflow)
            assert report.font_rebuilds == 1
            assert abs(report.ui_scale-report.monitor_scale/report.framebuffer_scale*1.25) < 1e-5
            if (report.framebuffer_width, report.framebuffer_height) != (requested.width, requested.height):
                raise RuntimeError('desktop must fit the requested 1440x900 replay window at 100% scale')
            if args.software_gl and 'llvmpipe' not in renderer.lower():
                raise RuntimeError('GUI check did not use the required software renderer: '+renderer)
        report = dict(schema='qpx.installed-workflow/1', python=sys.version, executable=sys.executable,
                      native=str(native), native_sha256=hashlib.sha256(native.read_bytes()).hexdigest(),
                      source_tree_sha256=q.build_environment().source_tree_sha256,
                      gui=q.has_gui, geos=q.has_geos, openslide=q.has_openslide, gl_renderer=renderer,
                      openslide_version=reader_version,
                      openslide_runtime_sha256=reader_runtime_sha256,
                      docked_workspace=report.docked_workspace if args.gui else False,
                      layout_saved=report.layout_saved if args.gui else False,
                      monitor_scale=report.monitor_scale if args.gui else None,
                      framebuffer_scale=report.framebuffer_scale if args.gui else None,
                      ui_scale=report.ui_scale if args.gui else None,
                      font_rebuilds=report.font_rebuilds if args.gui else None,
                      ui_stacked_fields=report.ui_stacked_fields if args.gui else None,
                      ui_wrapped_buttons=report.ui_wrapped_buttons if args.gui else None,
                      ui_overflow_items=report.ui_overflow_items if args.gui else None,
                      ui_max_overflow=report.ui_max_overflow if args.gui else None,
                      framebuffer=[report.framebuffer_width, report.framebuffer_height] if args.gui else None,
                      raw_archive_records=1,raw_replay_verified=raw_verified,raw_input_sha256=raw_result.input_sha256,raw_batch_reused=raw_batch_reused,
                      roi_samples=result.samples, native_batch_checkpoints=len(checkpoints))
    args.output.write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
