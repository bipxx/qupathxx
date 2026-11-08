"""Fixed workflow profile; verify the explicitly selected frozen runtime identity."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('slide', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('runtime', type=Path)
    parser.add_argument('--expected-tree', default='2ddb06c5da4de69aaf040253e30958cdd105f1a0e7e51aba82388eff4125fa13')
    parser.add_argument('--context',type=int,default=0)
    parser.add_argument('--label')
    mode=parser.add_mutually_exclusive_group()
    mode.add_argument('--resource-only',action='store_true')
    mode.add_argument('--steady',action='store_true',help='Six warmups + six timed workflows, no mask oracles')
    args = parser.parse_args()
    args.output.mkdir(exist_ok=False)
    (args.output / 'pid.txt').write_text(str(os.getpid()))
    sys.path.insert(0, str(args.runtime.resolve()))
    import numpy as np
    import qupathxx as q
    assert q.build_environment().source_tree_sha256 == args.expected_tree
    if args.expected_tree == '2ddb06c5da4de69aaf040253e30958cdd105f1a0e7e51aba82388eff4125fa13':
        assert q.build_environment().git_revision == '3155df804a778daf2d470b3ea60363630e735c98'
    roi = (29185, 2048, 512, 512)
    runs = []
    progress = (args.output / 'progress.jsonl').open('w', encoding='utf-8')
    for iteration in range(1 if args.resource_only else 12 if args.steady else 4):
        folder = args.output / str(iteration)
        folder.mkdir()
        stages = {}

        def timed(name, operation):
            wall, cpu = time.perf_counter(), time.process_time()
            result = operation()
            stages[name] = dict(wall_ms=(time.perf_counter()-wall)*1000,
                                cpu_ms=(time.process_time()-cpu)*1000)
            progress.write(json.dumps(dict(iteration=iteration, stage=name, **stages[name]))+'\n')
            progress.flush()
            return result

        started = time.perf_counter()

        def open_image():
            project = q.Project()
            doc = q.Document(q.open_slide(str(args.slide.resolve())))
            image = project.add_document(doc, 'CMU-1 ROI', str(args.slide.resolve()), 'openslide')
            return project, doc, image

        project, doc, image = timed('open_image', open_image)
        target = timed('setup_roi', lambda: doc.add_rectangle(q.Rect(*roi), 'Analysis support'))
        transform = q.StainTransform([[.65, .70, .29], [.07, .99, .11]])
        pixels = timed('read_roi', lambda: doc.server.read_region(q.RegionRequest(roi[0], roi[1], 0, roi[2], roi[3])))

        def statistics():
            request = q.AnalysisRequest.with_intensity_statistics(doc, target, transform, q.AnalysisKind.OPTICAL_DENSITY_MEANS)
            result = q.analyze(request)
            return request, result

        analysis_request, analysis = timed('roi_statistics', statistics)
        timed('keep_statistics', lambda: doc.keep_analysis(analysis_request, analysis))
        parameters = q.GridCellParameters()
        parameters.processing = q.CellProcessingPolicy.QUPATH_070_V2
        parameters.units = q.GridCellUnits.LEVEL_ZERO_PIXELS
        parameters.threshold = .4
        parameters.sigma = 1
        parameters.min_nucleus_area = 16
        parameters.max_nucleus_area = 512
        parameters.cell_expansion = 5
        parameters.max_cells = 50000
        parameters.work_bytes = 512*1024*1024
        contours = parameters.contours
        contours.work_bytes = 128*1024*1024
        parameters.contours = contours
        request = timed('cell_capture', lambda: q.CellRequest.for_nominal_roi(
            doc, target, transform, 1, args.context, args.context, parameters, q.RgbNominalSampling.AFFINE_INVERSE))
        result = timed('cell_detection', lambda: q.detect_cells(request))
        record = timed('keep_cells', lambda: doc.keep_cells(request, result))
        cells = timed('publish_cells', lambda: doc.publish_cell_record(record))

        def density():
            request = q.SpatialDensityRequest(doc, [target], [target], [], cells)
            result = q.measure_spatial_density(request)
            return request, result

        density_request, density_result = timed('density', density)
        density_record = timed('keep_density', lambda: doc.keep_density(density_request, density_result))
        timed('export_csv', lambda: q.save_spatial_density_csv(density_result, str(folder/'density.csv')))
        timed('save_project', lambda: project.save_new(str(folder/'study.qpxproj')))

        def reopen():
            restored = q.Project.open(str(folder/'study.qpxproj'))
            document = restored.document(image)
            return restored, document, document.density_record(density_record)

        restored, restored_doc, restored_density = timed('reopen_project', reopen)
        workflow_ms = (time.perf_counter()-started)*1000
        if args.resource_only or args.steady:
            assert restored_density.result.sha256==density_result.sha256
            row=density_result.row(0);assert row.count==result.cell_count
            runs.append(dict(iteration=iteration,warmup=args.steady and iteration<6,
                stages=stages,workflow_ms=workflow_ms,cells=result.cell_count,oracles=False,
                density=row.density_per_mm2,area_mm2=row.area_mm2,
                rgb_sum=[int(v) for v in analysis.intensity_statistics.rgb.sum],
                pixel_width_um=doc.pixel_width_um,pixel_height_um=doc.pixel_height_um))
            continue
        # Correctness/oracle work starts after the workflow timer.
        rgba = pixels.numpy()
        assert np.all(rgba[..., 3] == 255)
        rgb = rgba[..., :3].copy()
        sums = [int(v) for v in rgb.astype(np.uint64).sum(axis=(0, 1))]
        assert list(analysis.intensity_statistics.rgb.sum) == sums
        assert restored_density.result.sha256 == density_result.sha256
        assert q.spatial_density_csv(restored_density.result) == q.spatial_density_csv(density_result)
        assert density_result.row(0).count == len(cells) == result.cell_count
        width, height = roi[2:]
        masks = {}
        for name, raster in [('nucleus', result.nuclei), ('body', result.cells)]:
            labels = raster.labels_numpy()
            sampling = raster.nominal_sampling
            core = labels[sampling.context_y:sampling.context_y+height, sampling.context_x:sampling.context_x+width]
            core.astype('<u4').tofile(folder/(name+'.u32'))
            masks[name] = hashlib.sha256(core.astype('<u4').tobytes()).hexdigest()
        rgb.tofile(folder/'rgb.u8')
        row = density_result.row(0)
        value = dict(iteration=iteration, warmup=iteration==0, stages=stages, workflow_ms=workflow_ms,
            cells=result.cell_count, density=row.density_per_mm2, area_mm2=row.area_mm2,
            rgb_sha256=hashlib.sha256(rgb.tobytes()).hexdigest(), rgb_sum=sums,
            mask_sha256=masks, pixel_width_um=doc.pixel_width_um, pixel_height_um=doc.pixel_height_um,
            project_bytes=(folder/'study.qpxproj').stat().st_size,
            nucleus_pixels=int(np.count_nonzero(result.nuclei.labels_numpy())),
            algorithm=result.algorithm, saved_density_sha256=restored_density.result.sha256)
        runs.append(value)
        print(json.dumps(dict(iteration=iteration, cells=result.cell_count, workflow_ms=workflow_ms)), flush=True)
    environment = q.build_environment()
    label='candidate5' if environment.git_revision=='3155df804a778daf2d470b3ea60363630e735c98' else 'development-archive-fix'
    if args.label:label=args.label
    (args.output/'report.json').write_text(json.dumps(dict(engine=label,
        source_revision=environment.git_revision, source_tree_sha256=environment.source_tree_sha256,
        roi=roi, context=args.context, runs=runs), indent=2), encoding='utf-8')
    progress.close()


if __name__ == '__main__':
    main()
