"""Install and exercise frozen Windows candidates; never build the engine."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import venv
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).parent))
import assets


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def run(command, log, env=None):
    with log.open('w', encoding='utf-8') as stream:
        result = subprocess.run([str(x) for x in command], stdout=stream,
                                stderr=subprocess.STDOUT, env=env, timeout=120)
    if result.returncode:
        raise RuntimeError(f'{log.name}: exit {result.returncode}; inspect retained log')
    print(log.name + ': passed', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'win32' or sys.version_info[:2] != (3, 12):
        parser.error('requires Windows x64 CPython 3.12')
    args.input, args.work = args.input.resolve(), args.work.resolve()
    args.work.mkdir(parents=True, exist_ok=False)
    checkpoint = json.loads((ROOT/'docs/benchmarks/windows-workflow-checkpoint/checkpoint.json').read_text())
    names = {'licensed_wheel_sha256': 'qupathxx-0.1.0-1-cp312-cp312-win_amd64.whl',
             'native_archive_sha256': 'qupathxx-windows-x64-candidate.zip',
             'source_archive_sha256': 'qupathxx-source-6219a08f.zip'}
    for key, name in names.items():
        if sha(args.input/name) != checkpoint['package'][key]:
            raise ValueError('candidate hash mismatch: '+name)
    python = args.work/'environment/Scripts/python.exe'
    venv.EnvBuilder(with_pip=True).create(args.work/'environment')
    run([python, '-I', '-m', 'pip', 'install', '--disable-pip-version-check',
         str(args.input/names['licensed_wheel_sha256'])+'[arrays]', 'numpy==2.5.4'], args.work/'install.log')
    package = args.work/'environment/Lib/site-packages/qupathxx'
    native = args.work/'native'
    with zipfile.ZipFile(args.input/names['native_archive_sha256']) as archive:
        archive.extractall(native)  # Authenticated, owned candidate archive.
    mesa = assets.mesa(args.work/'test-assets')
    run([python, '-I', ROOT/'tools/ci/assets.py', '--software-python', args.work/'software-python',
         '--mesa-root', mesa], args.work/'software-python.log')
    driver_paths = []
    for directory in [package, native]:
        for name in ['opengl32.dll', 'libgallium_wgl.dll']:
            shutil.copy2(mesa/name, directory/name)
            driver_paths.append(str(directory/name))
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([str(Path(env['SystemRoot'])/'System32'), env['SystemRoot']])
    env['LIBGL_ALWAYS_SOFTWARE'], env['GALLIUM_DRIVER'] = '1', 'llvmpipe'
    software = args.work/'software-python/python.exe'
    fixtures = json.loads((ROOT/'tests/fixtures/manifest.json').read_text())
    fixture = next(x for x in fixtures['downloads'] if x['filename']=='CMU-1.svs')
    slide = assets.download(fixture, args.work/'fixtures')
    sdk = args.work/'sdk'
    run([software, '-I', ROOT/'examples/region_workflow.py', slide, sdk,
         '--roi', '29185', '2048', '512', '512'], args.work/'sdk.log', env)
    run([software, '-I', ROOT/'docs/benchmarks/windows-workflow-checkpoint/installed_desktop.py',
         slide, args.work/'desktop'], args.work/'desktop.log', env)
    run([native/'qupathxx_app.exe', '--project', sdk/'study.qpxproj', '--smoke', '--compact'],
        args.work/'native-open.log', env)
    probe = r'''
import ctypes, importlib.metadata as m, json
from pathlib import Path
import qupathxx as q
assert q.build_environment().git_revision == '6219a08f1713d641633156e70f12b3fd70331d82'
assert m.metadata('qupathxx')['License-Expression']=='AGPL-3.0-only'
kernel=ctypes.WinDLL('kernel32',use_last_error=True)
kernel.GetModuleHandleW.argtypes=[ctypes.c_wchar_p];kernel.GetModuleHandleW.restype=ctypes.c_void_p
kernel.GetModuleFileNameW.argtypes=[ctypes.c_void_p,ctypes.c_wchar_p,ctypes.c_uint32]
mapped={}
for name in ['geos.dll','geos_c.dll','libopenslide-1.dll']:
 handle=kernel.GetModuleHandleW(name);assert handle,name
 buf=ctypes.create_unicode_buffer(32768);assert kernel.GetModuleFileNameW(handle,buf,len(buf))
 assert Path(buf.value).parent.resolve()==Path(q.__file__).parent.resolve(),buf.value
 mapped[name]=buf.value
print(json.dumps(dict(mapped=mapped,source=q.build_environment().git_revision),indent=2))
'''
    run([software, '-I', '-c', probe], args.work/'mapped-runtime.log', env)
    protected = {}
    for folder in [sdk, args.work/'desktop']:
        for path in folder.iterdir():
            if path.suffix in {'.qpxproj', '.csv', '.json'}:
                protected[str(path)] = sha(path)
    result = dict(scope='fresh Windows hosted runner, declared MSVC runtime and test-only pinned Mesa; sanitized PATH',
                  runner_image=os.environ.get('ImageVersion'), candidate_hashes=checkpoint['package'],
                  installed_python=str(python), package=str(package), native=str(native),
                  test_driver_files=driver_paths, protected_user_files=protected,
                  sdk=json.loads((sdk/'workflow.json').read_text()),
                  desktop=json.loads((args.work/'desktop/desktop-report.json').read_text()),
                  install_run_verified=True, uninstall_verified=False, engine_builds=0)
    (args.work/'handoff.json').write_text(json.dumps(result, indent=2))
    print('Install/run complete; removal step must finish handoff.json', flush=True)


if __name__ == '__main__':
    main()
