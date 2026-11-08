"""Finite installed-file/DLL-origin check; no build or analysis campaign."""
import ctypes
import hashlib
import json
from pathlib import Path
import sys
import zipfile
import qupathxx as q

work = Path(sys.argv[1]).resolve()
wheel = work / 'delivery/qupathxx-0.1.0-6-cp312-cp312-win_amd64.whl'
package = Path(q.__file__).parent.resolve()
verified = []
with zipfile.ZipFile(wheel) as source:
    for name in source.namelist():
        if not name.endswith('.dist-info/RECORD'):
            assert (package.parent / name).read_bytes() == source.read(name), name
            verified.append(name)
q.Roi.polygon([q.Point(0, 0), q.Point(1, 0), q.Point(1, 1), q.Point(0, 1)])
kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.GetModuleHandleW.argtypes = [ctypes.c_wchar_p]
kernel.GetModuleHandleW.restype = ctypes.c_void_p
kernel.GetModuleFileNameW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_uint32]
origins = {}
for name in ['geos.dll', 'geos_c.dll', 'libopenslide-1.dll']:
    handle = kernel.GetModuleHandleW(name)
    assert handle, name
    text = ctypes.create_unicode_buffer(32768)
    assert kernel.GetModuleFileNameW(handle, text, len(text))
    assert Path(text.value).parent.resolve() == package, text.value
    origins[name] = text.value
environment = q.build_environment()
assert environment.source_tree_sha256 == '2875ca45645052c72636340337212c0ebdd5dba44057cfd237cefc5be38a2f2e'
result = dict(source_revision=environment.git_revision, source_tree_sha256=environment.source_tree_sha256,
    wheel_sha256=hashlib.sha256(wheel.read_bytes()).hexdigest(), installed_package=str(package),
    immutable_members_verified=len(verified), verified_members=verified, mapped_dlls=origins)
(work / 'delivery/installation.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps({name: value for name, value in result.items() if name != 'verified_members'}, indent=2))
