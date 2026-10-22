"""Direct C API: python reader-stack-probe.py LIBRARY OUTPUT_JSON FIXTURE_DIRECTORY."""
import ctypes as c,hashlib,json,sys,time
from pathlib import Path
library=Path(sys.argv[1]).resolve();root=Path(sys.argv[3]).resolve()
lib=c.CDLL(str(library))
lib.openslide_get_version.restype=c.c_char_p
lib.openslide_open.argtypes=[c.c_char_p];lib.openslide_open.restype=c.c_void_p
lib.openslide_close.argtypes=[c.c_void_p]
lib.openslide_get_error.argtypes=[c.c_void_p];lib.openslide_get_error.restype=c.c_char_p
lib.openslide_read_region.argtypes=[c.c_void_p,c.POINTER(c.c_uint32),c.c_int64,c.c_int64,c.c_int32,c.c_int64,c.c_int64]
report=dict(version=lib.openslide_get_version().decode(),library_sha256=hashlib.sha256(library.read_bytes()).hexdigest(),cases=[])
fixtures=json.loads((root/'fixtures.json').read_text(encoding='utf8'))
for case in fixtures['cases']:
    handle=lib.openslide_open(str(root/case['path']).encode());assert handle
    try:
        for level,info in enumerate(case['levels']):
            w,h=info['width'],info['height'];pixels=(c.c_uint32*(w*h))();start=time.perf_counter()
            lib.openslide_read_region(handle,pixels,0,0,level,w,h);elapsed=(time.perf_counter()-start)*1000
            error=lib.openslide_get_error(handle)
            rgba=b''.join(bytes([(p>>16)&255,(p>>8)&255,p&255,(p>>24)&255]) for p in pixels)
            expected=(root/info['expected']).read_bytes()
            report['cases'].append(dict(name=case['path'],level=level,width=w,height=h,elapsed_ms=elapsed,error=error.decode() if error else None,mismatched_bytes=sum(a!=b for a,b in zip(rgba,expected)),rgba_sha256=hashlib.sha256(rgba).hexdigest(),expected_sha256=hashlib.sha256(expected).hexdigest()))
    finally:lib.openslide_close(handle)
Path(sys.argv[2]).write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps(report,indent=2))
