if not __debug__: raise RuntimeError('Product proof requires assertions enabled')
from pathlib import Path
import hashlib, json, sys, zipfile
import qupathxx as q
from qupathxx import _native
r = Path(__file__).resolve().parents[2]
out = r / 'build/spatial-frontends-268'
m = json.loads((out / 'compiler-inputs-corrected.json').read_text(encoding='utf-8'))
wheel, = list((out / 'final-wheel').glob('*.whl'))
package = Path(q.__file__).parent
assert package.parent.resolve() == Path(sys.prefix, 'Lib/site-packages').resolve()
assert q.has_tiff and q.has_project and q.has_classifier_training and q.has_gui and q.has_geos and (not q.has_openslide)
environment = q.build_environment()
assert environment.git_revision == m['source'] and environment.source_tree_sha256 == m['fingerprint']
entries = []
with zipfile.ZipFile(wheel) as z:
    for member in z.namelist():
        if not member.startswith('qupathxx/') or member.endswith('/'):
            continue
        data = z.read(member)
        p = package.parent / member
        assert p.read_bytes() == data, member
        entries.append(dict(path=member, bytes=len(data), sha256=hashlib.sha256(data).hexdigest()))
names = json.loads((r / 'build/rawpython229-public-api.json').read_text(encoding='utf-8'))
assert len(names) == 71 and all((hasattr(q, n) and n in q.__all__ for n in names))
o = q.ProjectBatchOptions()
assert o.raw_pixel_model is None and o.raw_pixel_x is None and (o.raw_pixel_y is None) and isinstance(o.raw_pixels, q.RawPixelStreamOptions)
assert q.ProjectBatchKind.RAW_PIXELS.value == 5 and q.ProjectBatchKind.RAW.value == 4
result = dict(package=str(package), native=str(_native.__file__), python=sys.version, source=environment.git_revision, fingerprint=environment.source_tree_sha256, wheel=str(wheel), wheel_bytes=wheel.stat().st_size, wheel_sha256=hashlib.sha256(wheel.read_bytes()).hexdigest(), wheel_members=entries, new_public_exports=names, shared_test_dependencies=str(r / '.venv/Lib/site-packages'), gui=True, geos=True, training=True, tiff=True, project=True)
assert all((hasattr(q, n) and n in q.__all__ for n in ['RawPixelTrainingSettings', 'GuiRawPixelOptions', 'GuiRawPixelReport']))
assert hasattr(q.ViewerSession, 'contour_raw_pixels')
required = ['qupathxx/geos.dll', 'qupathxx/geos_c.dll', 'qupathxx/licenses/geos/COPYING', 'qupathxx/licenses/imgui/LICENSE.txt', 'qupathxx/licenses/glfw/LICENSE.md', 'qupathxx/licenses/nanobind/LICENSE']
with zipfile.ZipFile(wheel) as z:
    assert all((n in z.namelist() for n in required))
    for n in ['LICENSE', 'softfloat-notice.txt', 'zlib-notice.txt', 'SOURCE-NOTICES.txt']:
        assert z.read('qupathxx/licenses/opencv/' + n) == (r / 'build/deps/opencv-sdk/share/licenses/opencv' / n).read_bytes()
    assert z.read('qupathxx/licenses/geos/COPYING') == (r / 'build/deps/geos-sdk/share/licenses/geos/COPYING').read_bytes()
import ctypes
kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.GetModuleHandleW.argtypes = [ctypes.c_wchar_p]
kernel.GetModuleHandleW.restype = ctypes.c_void_p
kernel.GetModuleFileNameW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_uint]
kernel.GetModuleFileNameW.restype = ctypes.c_uint
loaded = {}
for name in ['geos.dll', 'geos_c.dll']:
    handle = kernel.GetModuleHandleW(name)
    assert handle
    buffer = ctypes.create_unicode_buffer(32768)
    assert kernel.GetModuleFileNameW(handle, buffer, len(buffer))
    path = Path(buffer.value).resolve()
    assert path.parent == package.resolve()
    assert path.read_bytes() == (r / 'build/deps/geos-sdk/bin' / name).read_bytes()
    loaded[name] = str(path)
result['loaded_geos'] = loaded
result['geos_version'] = q.geos_version

assert all(hasattr(q,n) and n in q.__all__ for n in ['RawPixelStreamContourOptions','RawPixelStreamContours','RawPixelStreamContourJob','trace_raw_pixel_stream','publish_raw_pixel_stream_contours'])
print('Final installed wheel members/source/runtime/capabilities, inherited exports and five new exports authenticated')

assert hasattr(q.GuiRawPixelOptions(),'stream_contours') and hasattr(q.ViewerSession,'contour_raw_pixel_stream')

assert q.ProjectBatchKind.RAW_PIXEL_CONTOURS.value==6 and isinstance(q.ProjectBatchOptions().raw_contours,q.RawPixelStreamContourOptions)

assert all(hasattr(q,n) and n in q.__all__ for n in ["RawPixelFileContours","trace_raw_pixel_stream_file","capture_raw_pixel_stream_replay","trace_raw_pixel_stream_replay"])
assert hasattr(q.ViewerSession,"contour_raw_pixel_file") and hasattr(q.RawPixelStreamContourJob,"request_replay") and hasattr(q.RawPixelStreamFileReadJob,"trace_path")

(out / 'installed-proof.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
names=['GuiSpatialOptions','GuiSpatialReport','SpatialDistanceUnits','SpatialDistanceOptions','SpatialObjectSnapshot','SpatialDistanceRequest','SpatialDistanceRow','SpatialDistanceResult','SpatialDistanceRecord','SpatialDistanceStats','SpatialDistanceJob','measure_spatial_distances','spatial_distance_csv','save_spatial_distance_csv']
assert all(hasattr(q,n) and n in q.__all__ for n in names)
assert q.SpatialDistanceJob().result is None
result['spatial_exports']=names
(out / 'installed-proof.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print('Complete installed spatial product proof passed')
