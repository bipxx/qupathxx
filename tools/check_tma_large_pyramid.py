"""Fresh-process Windows TMA probe; writes observations before checking limits."""
import argparse
import ctypes
import hashlib
import json
import math
from pathlib import Path
import sys
import time

CONTRACT=Path(__file__).resolve().parents[1]/'tests/fixtures/tma-large-pyramid-v1/contract.json'


def fixture_identity(path,c):
    manifest=json.loads(path.with_suffix(path.suffix+'.json').read_text(encoding='utf-8'))
    digest=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):
            digest.update(block)
    canonical=hashlib.sha256(json.dumps(c,sort_keys=True,separators=(',',':')).encode('utf-8')).hexdigest()
    if (manifest['bytes']!=path.stat().st_size or manifest['sha256']!=digest.hexdigest()
            or manifest['contract_sha256']!=canonical):
        raise ValueError('Fixture bytes or generator contract do not match manifest')
    return manifest


def process_memory():
    # https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters
    # https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getprocessmemoryinfo
    if sys.platform!='win32':
        raise RuntimeError('This probe requires Windows OS peak-working-set counters')
    from ctypes import wintypes
    class Counters(ctypes.Structure):
        _fields_=[('cb',wintypes.DWORD),('PageFaultCount',wintypes.DWORD)]+[
            (name,ctypes.c_size_t) for name in ('PeakWorkingSetSize','WorkingSetSize',
            'QuotaPeakPagedPoolUsage','QuotaPagedPoolUsage','QuotaPeakNonPagedPoolUsage',
            'QuotaNonPagedPoolUsage','PagefileUsage','PeakPagefileUsage')]
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.GetCurrentProcess.restype=wintypes.HANDLE
    psapi=ctypes.WinDLL('psapi',use_last_error=True)
    psapi.GetProcessMemoryInfo.argtypes=[wintypes.HANDLE,ctypes.POINTER(Counters),wintypes.DWORD]
    psapi.GetProcessMemoryInfo.restype=wintypes.BOOL
    counters=Counters();counters.cb=ctypes.sizeof(counters)
    if not psapi.GetProcessMemoryInfo(kernel.GetCurrentProcess(),ctypes.byref(counters),counters.cb):
        raise ctypes.WinError(ctypes.get_last_error())
    return dict(peak_working_set_bytes=counters.PeakWorkingSetSize,
                working_set_bytes=counters.WorkingSetSize,peak_commit_bytes=counters.PeakPagefileUsage)


def observe(path,c):
    import qupathxx as q
    import qupathxx._native as native
    options=q.TmaDearrayOptions()
    options.rows,options.columns=c['rows'],c['columns']
    options.core_units=q.TmaCoreUnits.Micrometers
    options.core_width,options.core_height=c['requested_width_um'],c['requested_height_um']
    limits=c['acceptance']
    options.thumbnail.max_width,options.thumbnail.max_height=limits['preview_width'],limits['preview_height']
    options.thumbnail.source_pixel_limit=options.thumbnail.output_pixel_limit=limits['source_pixels']
    options.work_bytes=limits['work_bytes']
    reader=q.TiffOpenOptions();reader.cache_bytes=limits['cache_bytes']
    fields=('decoded_blocks','decoded_bytes','cache_hits','directory_switches','cache_bytes','peak_cache_bytes')
    server=q.open_tiff(str(path),reader)
    try:
        before={key:getattr(server.tiff_read_stats,key) for key in fields}
        start=time.perf_counter()
        result=q.detect_tma_grid(server,options)
        elapsed=time.perf_counter()-start
        after={key:getattr(server.tiff_read_stats,key) for key in fields}
        plan=result.thumbnail_plan
        env=q.build_environment()
        return dict(schema='qpx.tma-large-pyramid-observation/1',
            fixture=str(path.resolve()),
            contract_sha256=hashlib.sha256(CONTRACT.read_bytes()).hexdigest(),
            engine=dict(revision=env.git_revision,fingerprint=env.source_tree_sha256,
                        module=str(native.__file__),module_sha256=hashlib.sha256(Path(native.__file__).read_bytes()).hexdigest()),
            image_width=result.image_width,image_height=result.image_height,
            plan={key:getattr(plan,key) for key in ('level','width','height','source_width','source_height','source_pixels','level_downsample')},
            core_size={key:getattr(result.core_size,key) for key in ('width','height','width_pixels','height_pixels','pixel_width_um','pixel_height_um','calibration_from_reader')},
            cores=[dict(center=[core.center.x,core.center.y],observed=core.observed,missing=wire.missing)
                   for core,wire in zip(result.cores,result.grid.cores,strict=True)],
            before=before,after=after,estimated_work_bytes=result.estimated_work_bytes,
            payload_bytes=result.payload_bytes,elapsed_seconds=elapsed,
            input_sha256=result.input_sha256,recipe_sha256=result.recipe_sha256,
            memory=process_memory())
    finally:
        server.close()


def assess(data,c,revision,fingerprint):
    """Return every failed criterion; no tolerance inferred from observations."""
    failures=[]
    def require(ok,message):
        if not ok: failures.append(message)
    a=c['acceptance'];p=data['plan'];size=data['core_size']
    require(data['engine']['revision']==revision,'engine revision')
    require(data['engine']['fingerprint']==fingerprint,'compiler fingerprint')
    require((data['image_width'],data['image_height'])==(c['width'],c['height']),'base dimensions')
    require(p['level']==a['level'] and p['level_downsample']==64,'natural level')
    require((p['width'],p['height'],p['source_width'],p['source_height'])==
            (a['preview_width'],a['preview_height'],a['preview_width'],a['preview_height']),'bounded preview dimensions')
    require(p['source_pixels']==a['source_pixels'],'source pixels')
    require(size==dict(width=c['requested_width_um'],height=c['requested_height_um'],
        width_pixels=c['core_width'],height_pixels=c['core_height'],pixel_width_um=c['pixel_width_um'],
        pixel_height_um=c['pixel_height_um'],calibration_from_reader=True),'physical calibration')
    for key in ('decoded_blocks','decoded_bytes'):
        require(data['before'][key]==0,'cold reader '+key)
        require(data['after'][key]==a[key],'bounded reader '+key)
    require(0<=data['after']['cache_bytes']<=data['after']['peak_cache_bytes']<=a['cache_bytes'],'cache peak')
    require(0<data['payload_bytes']<=data['estimated_work_bytes']<=a['work_bytes'],'native work admission')
    require(0<data['memory']['peak_working_set_bytes']<=a['process_peak_bytes'],'process peak working set')
    require(len(data['cores'])==len(c['cores']),'core count')
    observed=0
    for index,(actual,expected) in enumerate(zip(data['cores'],c['cores'])):
        x,y=actual['center'];ex,ey=expected['center']
        require(math.isfinite(x) and math.isfinite(y) and math.hypot(x-ex,y-ey)<=a['max_center_error_pixels'],f'center {index}')
        require(actual['missing']==(not expected['present']),f'missing flag {index}')
        require(actual['observed']==expected['present'],f'observed flag {index}')
        observed+=actual['observed']
    require(observed==a['observed_cores'],'observed count')
    return failures


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('fixture',type=Path);parser.add_argument('output',type=Path)
    parser.add_argument('--revision',required=True);parser.add_argument('--fingerprint',required=True)
    args=parser.parse_args()
    if args.output.exists(): raise FileExistsError(args.output)
    c=json.loads(CONTRACT.read_text(encoding='utf-8'))
    fixture=fixture_identity(args.fixture,c)
    data=observe(args.fixture,c)
    data['fixture_manifest']=fixture
    data['failures']=assess(data,c,args.revision,args.fingerprint)
    data['accepted']=not data['failures']
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes((json.dumps(data,indent=2,allow_nan=False)+'\n').encode())
    print(json.dumps(data,indent=2,allow_nan=False))
    if data['failures']: raise SystemExit(1)


if __name__=='__main__': main()
