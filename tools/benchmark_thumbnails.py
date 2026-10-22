"""Installed-wheel thumbnail selection/timing evidence; no speedup assertion."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import statistics
import sys
import time
import qupathxx as q


def file_sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024*1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--slide', type=Path, action='append', default=[])
    parser.add_argument('--runs', type=int, default=7)
    parser.add_argument('--source-pixel-limit', type=int, default=4*1024*1024)
    args = parser.parse_args()
    if not 3 <= args.runs <= 100:
        parser.error('--runs must be 3..100')
    options = q.ThumbnailOptions()
    options.max_width, options.max_height = 250, 150
    options.source_pixel_limit = args.source_pixel_limit
    options.validate()
    cases = []
    dense = q.Document(q.Image(2048, 2048, [30, 120, 210]*(2048*2048))).server
    for name, server, identity in [('dense-2048', dense, None)]+[
        (path.name, q.open_slide(str(path)), file_sha(path)) for path in args.slide
    ]:
        try:
            plan = q.plan_thumbnail(server.metadata, options)
        except ValueError as error:
            cases.append(dict(name=name, input_sha256=identity, base=[server.metadata.width, server.metadata.height],
                              state='unavailable', error=str(error)))
            server.close()
            continue
        samples = []
        for _ in range(args.runs):
            start = time.perf_counter()
            result = q.read_thumbnail(server, options)
            wall_ms = (time.perf_counter()-start)*1000
            samples.append(dict(wall_ms=wall_ms, native_ms=result.elapsed_ms,
                                rgba_sha256=hashlib.sha256(result.pixels.numpy().tobytes()).hexdigest()))
        assert len({sample['rgba_sha256'] for sample in samples}) == 1
        cases.append(dict(name=name, input_sha256=identity, base=[server.metadata.width, server.metadata.height],
                          level=plan.level, source=[plan.source_width, plan.source_height],
                          output=[plan.width, plan.height], source_rgba_bytes=plan.source_pixels*4,
                          output_rgba_bytes=plan.output_pixels*4,
                          median_wall_ms=statistics.median(s['wall_ms'] for s in samples), samples=samples))
        server.close()
    env = q.build_environment()
    report = dict(schema='qpx.thumbnail-benchmark/1', platform=platform.platform(),
                  python=sys.version, native_sha256=file_sha(Path(q._native.__file__)),
                  source_tree_sha256=env.source_tree_sha256, runs=args.runs,
                  policy=f'250x150, source limit {options.source_pixel_limit} pixels, default output budget, one complete natural level, encoded-RGB area filtering',
                  timing='whole synchronous call; repeated inputs may be warm; no speedup or RSS claim', cases=cases)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')
    print(json.dumps({case['name']: case.get('median_wall_ms', case.get('error')) for case in cases}, indent=2))


if __name__ == '__main__':
    main()
