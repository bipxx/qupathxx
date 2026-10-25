"""Check cold-reader refusal and same-reader recovery at exact resource limits.

The large-pyramid contract fixes geometry and maximum resource bounds. This
additional probe requires zero decoded bytes/blocks on four refused requests,
followed by successful detection at exact source/output/work admission.
"""
import argparse
import importlib.util
import json
from pathlib import Path

spec=importlib.util.spec_from_file_location('large_tma_probe',Path(__file__).with_name('check_tma_large_pyramid.py'))
probe=importlib.util.module_from_spec(spec);spec.loader.exec_module(probe)
KINDS=('source_pixel','output_pixel','work_byte','full_base_region')


def assess(data,c):
    failures=[]
    if [case['kind'] for case in data['cases']]!=list(KINDS):
        failures.append('named refusal cohort')
    for case in data['cases']:
        kind=case['kind']
        if case['exception_type'] not in ('ValueError','RuntimeError'):
            failures.append(kind+': refusal type')
        expected={'source_pixel':'source pixel limit','output_pixel':'output pixel limit',
                  'work_byte':'working payload budget','full_base_region':'256 MiB output limit'}
        if kind not in expected or expected[kind] not in case['exception_message']:
            failures.append(kind+': refusal reason')
        if any(case['after_refusal'][key]!=0 for key in ('decoded_blocks','decoded_bytes','cache_bytes','peak_cache_bytes')):
            failures.append(kind+': decoded before refusal')
        if case['closed_after_refusal']:
            failures.append(kind+': reader poisoned')
        if case['admitted_work_bytes']!=case['recovery']['estimated_work_bytes']:
            failures.append(kind+': exact work admission')
        if (case['recovery']['decoded_blocks'],case['recovery']['decoded_bytes'])!=(c['acceptance']['decoded_blocks'],c['acceptance']['decoded_bytes']):
            failures.append(kind+': recovery IO')
        if case['recovery']['input_sha256']!=data['baseline_input_sha256']:
            failures.append(kind+': recovery pixels')
        if not case['recovery']['geometry_and_flags_match']:
            failures.append(kind+': recovery geometry')
    return failures


def observe(path,c):
    import qupathxx as q
    cases=[]
    baseline=None
    for kind in KINDS:
        options=probe.detection_options(q,c)
        pixels=c['acceptance']['source_pixels']
        admitted=q.tma_dearray_work_bytes(c['acceptance']['preview_width'],c['acceptance']['preview_height'],options)+pixels*4
        options.work_bytes=admitted
        if kind=='source_pixel': options.thumbnail.source_pixel_limit=pixels-1
        elif kind=='output_pixel': options.thumbnail.output_pixel_limit=pixels-1
        elif kind=='work_byte': options.work_bytes=admitted-1
        reader=q.TiffOpenOptions();reader.cache_bytes=c['acceptance']['cache_bytes']
        server=q.open_tiff(str(path),reader)
        try:
            exception_type=exception_message=''
            try:
                if kind=='full_base_region':
                    server.read_region(q.RegionRequest(0,0,0,c['width'],c['height']))
                else:
                    q.detect_tma_grid(server,options)
            except (ValueError,RuntimeError) as error:
                exception_type=type(error).__name__;exception_message=str(error)
            stats=server.tiff_read_stats
            after={key:getattr(stats,key) for key in ('decoded_blocks','decoded_bytes','cache_bytes','peak_cache_bytes')}
            closed=server.closed
            options.thumbnail.source_pixel_limit=options.thumbnail.output_pixel_limit=pixels
            options.work_bytes=admitted
            recovery=q.detect_tma_grid(server,options)
            if baseline is None: baseline=recovery.input_sha256
            import math
            matches=len(recovery.cores)==len(c['cores']) and all(
                math.hypot(core.center.x-expected['center'][0],core.center.y-expected['center'][1])<=c['acceptance']['max_center_error_pixels']
                and core.observed==expected['present'] and wire.missing==(not expected['present'])
                for core,wire,expected in zip(recovery.cores,recovery.grid.cores,c['cores'],strict=True))
            stats=server.tiff_read_stats
            cases.append(dict(kind=kind,exception_type=exception_type,exception_message=exception_message,
                after_refusal=after,closed_after_refusal=closed,admitted_work_bytes=admitted,
                recovery=dict(estimated_work_bytes=recovery.estimated_work_bytes,decoded_blocks=stats.decoded_blocks,
                    decoded_bytes=stats.decoded_bytes,input_sha256=recovery.input_sha256,geometry_and_flags_match=matches)))
        finally:
            server.close()
    env=q.build_environment()
    return dict(schema='qpx.tma-large-pyramid-admission/1',cases=cases,baseline_input_sha256=baseline,
                engine=dict(revision=env.git_revision,fingerprint=env.source_tree_sha256),memory=probe.process_memory())


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('fixture',type=Path);parser.add_argument('output',type=Path)
    parser.add_argument('--revision',required=True);parser.add_argument('--fingerprint',required=True)
    args=parser.parse_args()
    if args.output.exists(): raise FileExistsError(args.output)
    c=json.loads(probe.CONTRACT.read_text(encoding='utf-8'))
    fixture=probe.fixture_identity(args.fixture,c)
    data=observe(args.fixture,c);data['fixture_manifest']=fixture
    data['failures']=assess(data,c)
    if data['engine']!=dict(revision=args.revision,fingerprint=args.fingerprint):data['failures'].append('engine identity')
    data['accepted']=not data['failures']
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes((json.dumps(data,indent=2,allow_nan=False)+'\n').encode())
    print(json.dumps(data,indent=2,allow_nan=False))
    if data['failures']: raise SystemExit(1)


if __name__=='__main__': main()
