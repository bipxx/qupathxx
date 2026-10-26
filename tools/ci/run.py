"""CI entry point: native configurations and clean installed-wheel workflow gates."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import assets
import scientific_grid_evidence as scientific
import tma_axis_evidence as axis
import tma_dearray_evidence as dearray
sys.path.insert(0, str(ROOT/'tools'))
import build_opencv_sdk


def qualification_environment(environ):
    """Admit pytest's own settings before building; Python -I does not bind them."""
    for name in ('PYTEST_ADDOPTS','PYTEST_PLUGINS'):
        if environ.get(name,'').strip():
            raise ValueError('qualification rejects inherited '+name)
    return dict(environ)


def run(args, log, env=None, timeout=2400, expected=0):
    print('+ '+' '.join(map(str, args)), flush=True)
    with Path(log).open('w', encoding='utf8') as stream:
        result = subprocess.run(list(map(str, args)), cwd=ROOT, env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=timeout)
    if result.returncode != expected:
        print(Path(log).read_text(encoding='utf8', errors='replace')[-16000:], flush=True)
        raise RuntimeError(f'command failed ({result.returncode}); log: {log}')


def ensure_geos(work):
    sdk = ROOT/'build/deps/geos-sdk'
    marker = sdk/'.ci-sdk.json'
    if marker.exists():
        saved = json.loads(marker.read_text(encoding='utf8'))
        if saved['source'] != 'd5e5192a686d065eaed082de14dd26244c5c8e02bff16b2c6cce3265f648e00e':
            raise ValueError('cached GEOS source differs')
        for name, digest in saved['files'].items():
            if assets.sha(sdk/name) != digest:
                raise ValueError('cached GEOS SDK changed: '+name)
    else:
        run([sys.executable, '-I', ROOT/'tools/build_geos_sdk.py', '--jobs', '2'], work/'geos-build.log')
        saved = dict(source='d5e5192a686d065eaed082de14dd26244c5c8e02bff16b2c6cce3265f648e00e',
                     files={p.relative_to(sdk).as_posix(): assets.sha(p) for p in sdk.rglob('*') if p.is_file()})
        marker.write_text(json.dumps(saved, indent=2)+'\n', encoding='utf8')
    return sdk


def ensure_opencv(work):
    sdk = ROOT/'build/deps/opencv-sdk'
    marker = sdk/'qupathxx-opencv-sdk.json'
    if not marker.exists():
        run([sys.executable, '-I', ROOT/'tools/build_opencv_sdk.py', '--jobs', '2'], work/'opencv-build.log')
    saved = json.loads(marker.read_text(encoding='utf8'))
    if (saved.get('schema') != 'qpx.opencv-sdk/1' or saved.get('version') != build_opencv_sdk.VERSION
            or saved.get('commit') != build_opencv_sdk.COMMIT or saved.get('source_sha256') != build_opencv_sdk.SHA256
            or saved.get('options') != build_opencv_sdk.OPTIONS or not saved.get('files')):
        raise ValueError('cached OpenCV SDK configuration differs')
    for name, digest in saved['files'].items():
        path = (sdk/name).resolve()
        if not path.is_relative_to(sdk.resolve()) or assets.sha(path) != digest:
            raise ValueError('cached OpenCV SDK changed: '+name)
    return sdk


def archive_windows(wheel, output):
    stage = output/'standalone'
    stage.mkdir()
    with zipfile.ZipFile(wheel) as source:
        for entry in source.infolist():
            name = Path(entry.filename)
            if name.parts[0] != 'qupathxx' or entry.is_dir():
                continue
            if name.suffix.lower() not in ['.exe', '.dll'] and 'licenses' not in name.parts:
                continue
            relative = Path(*name.parts[1:])
            target = stage/relative
            if not target.resolve().is_relative_to(stage.resolve()):
                raise ValueError('invalid wheel archive path')
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(source.read(entry))
    (stage/'README.txt').write_text(
        'qupathxx development archive. Requires Windows x64, the Microsoft Visual C++ 2015-2022 x64 runtime, '
        'and OpenGL 3.2 for the GUI. No Python installation is required by the executables.\n'
        'See bundled runtime notices. This is a CI candidate, not a supported release.\n', encoding='utf8')
    shutil.make_archive(str(output/'qupathxx-windows-x64-candidate'), 'zip', stage)
    return stage


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mode', choices=['minimal', 'native', 'sanitize', 'headless', 'gui'], required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--live-reference', action='store_true')
    parser.add_argument('--extended-readers', action='store_true')
    args = parser.parse_args()
    env = qualification_environment(os.environ)
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    (work/'pytest-environment.json').write_text(json.dumps({
        'schema':'qpx.pytest-environment/1',
        'PYTEST_ADDOPTS':env.get('PYTEST_ADDOPTS',''),
        'PYTEST_PLUGINS':env.get('PYTEST_PLUGINS',''),
        'PYTEST_DISABLE_PLUGIN_AUTOLOAD':env.get('PYTEST_DISABLE_PLUGIN_AUTOLOAD'),
    },indent=2)+'\n',encoding='utf-8')
    env.pop('PYTHONHOME', None)
    env.pop('PYTHONPATH', None)
    env['CMAKE_BUILD_PARALLEL_LEVEL'] = '2'
    full = args.mode != 'minimal'
    # Verify external inputs before spending time compiling the SDK and wheel.
    mesa = assets.mesa(ROOT/'build/ci/deps') if args.mode == 'gui' and os.name == 'nt' else None
    openslide = assets.openslide(ROOT/'build/ci/deps') if full and args.mode != 'sanitize' else None
    if args.mode in ['headless', 'gui']:
        run([sys.executable, '-I', ROOT/'tools/fetch_reader_fixtures.py'] +
            (['--extended'] if args.extended_readers else []), work/'fixtures.log', env)
        env['QUPATHXX_APERIO_FIXTURE'] = str(ROOT/'build/fixtures/CMU-1-Small-Region.svs')
        if args.extended_readers:
            env['QUPATHXX_APERIO_MULTILEVEL_FIXTURE'] = str(ROOT/'build/fixtures/CMU-1.svs')
            env['QUPATHXX_HAMAMATSU_FIXTURE'] = str(ROOT/'build/fixtures/CMU-1.ndpi')
    geos = ensure_geos(work) if full else None
    opencv = ensure_opencv(work) if full else None
    if geos:
        env['LD_LIBRARY_PATH'] = os.pathsep.join([str(geos/'lib'), str(geos/'lib64'), env.get('LD_LIBRARY_PATH', '')])
        if os.name != 'nt':
            env['QUPATHXX_GEOS_SDK'] = str(geos)
    flags = dict(QUPATHXX_BUILD_TESTS='ON', QUPATHXX_BUILD_GUI='ON' if args.mode == 'gui' else 'OFF',
                 QUPATHXX_BUILD_PYTHON='ON' if args.mode in ['headless', 'gui'] else 'OFF',
                 QUPATHXX_BUILD_GEOS='ON' if full else 'OFF',
                 QUPATHXX_BUILD_INTERCHANGE='ON' if full else 'OFF',
                 QUPATHXX_BUILD_PROJECT='ON' if full else 'OFF',
                 QUPATHXX_BUILD_CLASSIFIER_TRAINING='ON' if full else 'OFF',
                 QUPATHXX_BUILD_TIFF='ON' if full else 'OFF',
                 QUPATHXX_BUILD_OPENSLIDE='ON' if full and args.mode != 'sanitize' else 'OFF')
    if geos:
        flags['GEOS_ROOT'] = str(geos)
        if os.name != 'nt' and args.mode == 'headless':
            flags['QUPATHXX_BUNDLE_GEOS'] = 'ON'
    if opencv:
        flags['OpenCV_DIR'] = str(build_opencv_sdk.cmake_config_directory(opencv))
        flags['QUPATHXX_OPENCV_NOTICES'] = str(opencv/'share/licenses/opencv')
    if openslide:
        flags['OpenSlide_ROOT'] = str(openslide)
        if os.name != 'nt':
            flags['QUPATHXX_BUNDLE_OPENSLIDE'] = 'ON'
    if args.mode == 'sanitize':
        sanitizer = '-fsanitize=address,undefined -fno-omit-frame-pointer'
        flags.update(CMAKE_CXX_FLAGS=sanitizer, CMAKE_C_FLAGS=sanitizer, CMAKE_EXE_LINKER_FLAGS=sanitizer,
                     CMAKE_BUILD_TYPE='Debug')
        env['ASAN_OPTIONS'] = 'detect_leaks=1:halt_on_error=1'
        env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    build = work/'native-build'
    if args.mode in ['headless', 'gui']:
        wheel_dir = work/'wheels'
        command = [sys.executable, '-m', 'pip', 'wheel', '.', '--no-deps', '--no-build-isolation', '--wheel-dir', wheel_dir,
                   '--config-settings', f'build-dir={build}']
        for name, value in flags.items():
            command += ['--config-settings', f'cmake.define.{name}={value}']
        run(command, work/'wheel-build.log', env)
    else:
        command = ['cmake', '-S', ROOT, '-B', build]
        command += [f'-D{name}={value}' for name, value in flags.items()]
        run(command, work/'configure.log', env)
        run(['cmake', '--build', build, '--config', 'Debug' if args.mode == 'sanitize' else 'Release', '--parallel', '2'], work/'build.log', env)
    run(['ctest', '--test-dir', build, '-C', 'Debug' if args.mode == 'sanitize' else 'Release', '--output-on-failure', '--test-output-size-passed', '1048576',
         '--output-junit', work/'native-tests.xml'], work/'native-tests.log', env, 900)
    if args.mode not in ['headless', 'gui']:
        return
    wheel = next((work/'wheels').glob('qupathxx-*.whl'))
    if os.name == 'nt':
        manifest = work/'windows-wheel-runtime-manifest.json'
        inspect = [sys.executable, '-I', ROOT/'tools/ci/runtime_manifest.py', '--wheel', wheel]
        if args.mode == 'gui':
            inspect += ['--gui']
        run(inspect+['--output', manifest], work/'windows-wheel-runtime-manifest.log', env, 120)
        run(inspect+['--verify', manifest], work/'windows-wheel-runtime-verify.log', env, 120)
        run([sys.executable, '-I', ROOT/'tools/ci/probe_pe_forwarders.py', '--work', work/'native-forwarder-probe',
             '--output', work/'pe-forwarder-native.json'], work/'pe-forwarder-native.log', env, 180)

    install = work/'install'
    run([sys.executable, '-m', 'venv', install], work/'venv.log')
    python = install/('Scripts/python.exe' if os.name == 'nt' else 'bin/python')
    run([python, '-m', 'pip', 'install', '-r', ROOT/'tools/ci/requirements.txt', wheel], work/'install.log', env)
    test_python = python
    if os.name != 'nt':
        inspect = [sys.executable, '-I', ROOT/'tools/ci/linux_runtime_manifest.py', '--wheel', wheel]
        run(inspect+['--output', work/'linux-wheel-runtime-manifest.json'], work/'linux-wheel-runtime-manifest.log', env, 120)
        run(inspect+['--verify', work/'linux-wheel-runtime-manifest.json'], work/'linux-wheel-runtime-verify.log', env, 120)
        env.pop('LD_LIBRARY_PATH', None)
        run([python, '-I', ROOT/'tools/ci/check_linux_runtime.py', '--wheel', wheel,
             '--geos-sdk', geos, '--openslide-sdk', openslide, '--output', work/'linux-runtime.json'],
            work/'linux-runtime.log', env, 120)
    if args.mode == 'gui' and os.name == 'nt':
        run([python, '-I', ROOT/'tools/ci/assets.py', '--software-python', work/'software-python', '--mesa-root', mesa], work/'software-python.log')
        test_python = work/'software-python/python.exe'
        package = install/'Lib/site-packages/qupathxx'
        for name in ['opengl32.dll', 'libgallium_wgl.dll']:
            shutil.copy2(mesa/name, package/name)
        env['GALLIUM_DRIVER'] = 'llvmpipe'
        env['LIBGL_ALWAYS_SOFTWARE'] = '1'
    env['QUPATHXX_TEST_GUI'] = '1' if args.mode == 'gui' else '0'
    if args.live_reference:
        env['QUPATHXX_COMPARE_QUPATH'] = '1'
    command = [test_python, '-I', ROOT/'tools/ci/check_install.py', '--wheel', wheel, '--output', work/'installed-workflow.json',
               '--standalone-project', work/'standalone-input.qpxproj', '--openslide-version', '4.0.1']
    if os.name != 'nt':
        command += ['--openslide-runtime-sha256', assets.sha(openslide/'lib/libopenslide.so.1')]
    if args.mode == 'gui':
        command += ['--gui', '--software-gl']
    run(command, work/'installed-workflow.log', env)
    run([test_python, '-I', '-m', 'pytest', 'tests', '-q', '-o', 'faulthandler_timeout=60',
         f'--junitxml={work}/python-tests.xml'], work/'python-tests.log', env, 2400)
    run([test_python, '-I', ROOT/'tools/compare_qupath_cells.py', '--cell-algorithm', 'log-bodies',
         '--report', work/'qupath-cells-body-comparison.json'], work/'qupath-cells-body-comparison.log', env, 120)
    run([test_python, '-I', ROOT/'tools/compare_qupath_cells.py', '--cell-algorithm', 'log-sampled',
         '--report', work/'qupath-cells-sampled-comparison.json'], work/'qupath-cells-sampled-comparison.log', env, 120)
    if args.extended_readers:
        for padding in [0,32]:
            run([test_python, '-I', ROOT/'tools/compare_qupath_ndpi_cells.py',
                 '--image', env['QUPATHXX_HAMAMATSU_FIXTURE'],
                 '--reference', ROOT/'tests/fixtures/qupath-ndpi-cells-0.7.0/capture',
                 '--include-sampled', '--context-padding', str(padding),
                 '--report', work/f'qupath-ndpi-sampled-context{padding}.json'],
                 work/f'qupath-ndpi-sampled-context{padding}.log', env, 120)
    if args.extended_readers:
        run([test_python, '-I', ROOT/'tools/benchmark_vendor_reader.py',
             '--slide', env['QUPATHXX_APERIO_MULTILEVEL_FIXTURE'], '--output', work/'vendor-reader-benchmark.json'],
            work/'vendor-reader-benchmark.log', env, 120)
        run([test_python, '-I', ROOT/'tools/benchmark_vendor_reader.py',
             '--slide', env['QUPATHXX_HAMAMATSU_FIXTURE'], '--output', work/'ndpi-reader-benchmark.json'],
            work/'ndpi-reader-benchmark.log', env, 120)
        if args.mode == 'gui':
            run([test_python, '-I', ROOT/'tools/benchmark_gui_overview.py',
                 '--slide', env['QUPATHXX_APERIO_MULTILEVEL_FIXTURE'], '--output', work/'gui-overview-benchmark.json',
                 '--capture', work/'gui-overview.png'], work/'gui-overview-benchmark.log', env, 120)
    run([test_python, '-I', ROOT/'tools/benchmark_classifier.py', '--output', work/'classifier-benchmark.json'],
        work/'classifier-benchmark.log', env, 120)
    run([test_python, '-I', ROOT/'tools/benchmark_object_classifier.py', '--rows', '10000', '--output', work/'object-classifier-benchmark.json'],
        work/'object-classifier-benchmark.log', env, 120)
    run([test_python, '-I', ROOT/'tools/benchmark_classifier_archives.py', '--rows', '10000', '--output', work/'classifier-archive-benchmark.json'],
        work/'classifier-archive-benchmark.log', env, 120)
    run([test_python, '-I', ROOT/'tools/benchmark_pixel_classifier.py', '--output', work/'pixel-classifier-benchmark.json'],
        work/'pixel-classifier-benchmark.log', env, 120)
    run([test_python, '-I', ROOT/'tools/benchmark_pixel_workflow.py', '--output', work/'pixel-workflow-benchmark.json'],
        work/'pixel-workflow-benchmark.log', env, 120)
    run([test_python, '-I', ROOT/'tools/benchmark_pixel_stream.py', '--output', work/'pixel-stream-benchmark.json'],
        work/'pixel-stream-benchmark.log', env, 120)
    run([test_python, '-I', ROOT/'tools/benchmark_pixel_stream_file.py', '--file', work/'pixel-stream-file.qps', '--output', work/'pixel-stream-file-benchmark.json'],
        work/'pixel-stream-file-benchmark.log', env, 120)
    run([test_python, '-I', ROOT/'tools/benchmark_pixel_stream_read.py', '--file', work/'pixel-stream-file.qps', '--output', work/'pixel-stream-read-benchmark.json'],
        work/'pixel-stream-read-benchmark.log', env, 120)
    run([test_python, '-I', ROOT/'tools/benchmark_pixel_input_digest.py', '--output', work/'pixel-input-digest-benchmark.json'],
        work/'pixel-input-digest-benchmark.log', env, 120)
    run([test_python, '-I', ROOT/'tools/benchmark_project_pixel_batch.py', '--work', work/'project-pixel-batch', '--output', work/'project-pixel-batch-benchmark.json'],
        work/'project-pixel-batch-benchmark.log', env, 180)
    run([test_python, '-I', ROOT/'tools/probe_project_pixel_batch_cancel.py', '--work', work/'pixel-batch-interrupt', '--output', work/'pixel-batch-interrupt.json'],
        work/'pixel-batch-interrupt.log', env, 90)
    run([test_python, '-I', ROOT/'tools/benchmark_tiff_reader.py', '--output', work/'tiff-reader-benchmark.json'],
        work/'tiff-reader-benchmark.log', env, 120)
    run([test_python, '-I', ROOT/'tools/benchmark_raw_records.py', '--output', work/'raw-records-benchmark.json'],
        work/'raw-records-benchmark.log', env, 180)
    run([test_python, '-I', ROOT/'tools/benchmark_tma.py', '--output', work/'tma-benchmark.json'],
        work/'tma-benchmark.log', env, 120)
    run([test_python, '-I', ROOT/'tools/benchmark_pixel_stream_cli.py', '--work', work/'pixel-stream-cli', '--output', work/'pixel-stream-cli-benchmark.json'],
        work/'pixel-stream-cli-benchmark.log', env, 120)
    run([test_python, '-I', ROOT/'tools/benchmark_pixel_stream_records.py', '--file', work/'pixel-stream-file.qps', '--project', work/'pixel-stream-links.qpxproj', '--output', work/'pixel-stream-record-benchmark.json'],
        work/'pixel-stream-record-benchmark.log', env, 120)
    run([test_python, '-I', ROOT/'tools/benchmark_pixel_stream_file.py', '--job', '--file', work/'pixel-stream-job.qps', '--output', work/'pixel-stream-job-benchmark.json'],
        work/'pixel-stream-job-benchmark.log', env, 120)
    run([test_python, '-I', ROOT/'tools/benchmark_pixel_archives.py', '--output', work/'pixel-archive-benchmark.json'],
        work/'pixel-archive-benchmark.log', env, 120)
    if args.mode == 'gui':
        run([test_python, '-I', ROOT/'tools/benchmark_tiff_reader.py', '--gui', '--output', work/'gui-tiff-reader-benchmark.json', '--capture', work/'gui-tiff-reader.ppm'],
            work/'gui-tiff-reader-benchmark.log', env, 120)
        run([test_python, '-I', ROOT/'tools/benchmark_tma.py', '--gui', '--output', work/'gui-tma-benchmark.json', '--capture', work/'gui-tma.ppm'],
            work/'gui-tma-benchmark.log', env, 120)
        run([test_python, '-I', ROOT/'tools/benchmark_pixel_stream_records.py', '--gui', '--file', work/'pixel-stream-file.qps', '--project', work/'gui-pixel-stream-links.qpxproj', '--output', work/'gui-pixel-stream-record-benchmark.json'],
            work/'gui-pixel-stream-record-benchmark.log', env, 120)
        run([test_python, '-I', ROOT/'tools/benchmark_gui_pixel_stream_read.py', '--file', work/'pixel-stream-file.qps', '--output', work/'gui-pixel-stream-read-benchmark.json', '--capture', work/'gui-pixel-stream-read.ppm'],
            work/'gui-pixel-stream-read-benchmark.log', env, 120)
        run([test_python, '-I', ROOT/'tools/benchmark_gui_pixel_stream.py', '--file', work/'gui-pixel-stream.qps', '--output', work/'gui-pixel-stream-benchmark.json'],
            work/'gui-pixel-stream-benchmark.log', env, 120)
        run([test_python, '-I', ROOT/'tools/benchmark_gui_pixel.py', '--output', work/'gui-pixel-benchmark.json'],
            work/'gui-pixel-benchmark.log', env, 120)
        run([test_python, '-I', ROOT/'tools/benchmark_gui_pixel_contours.py', '--output', work/'gui-pixel-contour-benchmark.json'],
            work/'gui-pixel-contour-benchmark.log', env, 120)
        run([test_python, '-I', ROOT/'tools/benchmark_gui_pixel_archives.py', '--output', work/'gui-pixel-archive-benchmark.json'],
            work/'gui-pixel-archive-benchmark.log', env, 120)
        run([test_python, '-I', ROOT/'tools/benchmark_gui_pixel_archives.py', '--viewport', '--output', work/'gui-pixel-window-benchmark.json'],
            work/'gui-pixel-window-benchmark.log', env, 120)
    run([test_python, '-I', ROOT/'tools/generate_stubs.py', '--check'], work/'stubs.log', env)
    run([test_python, '-I', '-m', 'mypy', '--strict', '--no-incremental', *scientific.POSITIVE], work/'typing.log', env)
    scientific.positive_typing(work/'typing.log')
    for log,(filename,lines) in scientific.NEGATIVE.items():
        run([test_python,'-I','-m','mypy','--strict','--no-incremental','tests/typing/'+filename],work/log,env,expected=1)
        scientific.negative_typing(work/log,filename,lines)
    run([test_python,'-I','-m','mypy','--strict','--no-incremental','tests/typing/tma_axes_invalid.py'],work/axis.TYPING_LOG,env,expected=1)
    axis.typing(work/axis.TYPING_LOG)
    run([test_python,"-I","-m","mypy","--strict","--no-incremental","tests/typing/tma_dearray_valid.py"],work/dearray.CONTRACT["typing_positive_log"],env)
    for filename,contract in dearray.CONTRACT["typing_negative"].items():
        run([test_python,"-I","-m","mypy","--strict","--no-incremental","tests/typing/"+filename],work/contract["log"],env,expected=1)
    dearray.typing(work)
    negative_log = work/'typing-negative.log'
    run([test_python, '-I', '-m', 'mypy', '--strict', '--no-incremental', 'tests/typing/invalid_api.py'], negative_log, env, expected=1)
    lines = re.findall(r'invalid_api\.py:(\d+): error:', negative_log.read_text(encoding='utf8'))
    if sorted(map(int, lines)) != [6, 7, 8, 9, 11, 13, 14]:
        raise RuntimeError('negative typing fixture must reject all seven intended mistakes')
    qptma_negative_log=work/'qptma-typing-negative.log'
    run([test_python,'-I','-m','mypy','--strict','--no-incremental','tests/typing/qptma_invalid.py'],qptma_negative_log,env,expected=1)
    if sorted(map(int,re.findall(r'qptma_invalid\.py:(\d+): error:',qptma_negative_log.read_text(encoding='utf-8')))) != [4,5,6]:
        raise RuntimeError('QPTMA typing fixture must reject all three intended mistakes')
    hierarchy_negative_log=work/'hierarchy-typing-negative.log'
    run([test_python,'-I','-m','mypy','--strict','--no-incremental','tests/typing/hierarchy_invalid.py'],hierarchy_negative_log,env,expected=1)
    if sorted(map(int,re.findall(r'hierarchy_invalid\.py:(\d+): error:',hierarchy_negative_log.read_text(encoding='utf-8')))) != [4,5,7]:
        raise RuntimeError('hierarchy typing fixture must reject all three intended mistakes')
    if os.name == 'nt' and args.mode == 'gui':
        stage = archive_windows(wheel, work)
        inspect = [sys.executable, '-I', ROOT/'tools/ci/runtime_manifest.py', '--native-archive',
                   work/'qupathxx-windows-x64-candidate.zip', '--gui']
        run(inspect+['--output', work/'windows-native-runtime-manifest.json'], work/'windows-native-runtime-manifest.log', env, 120)
        run(inspect+['--verify', work/'windows-native-runtime-manifest.json'], work/'windows-native-runtime-verify.log', env, 120)

        native_test = work/'standalone-test'
        shutil.copytree(stage, native_test)
        for name in ['opengl32.dll', 'libgallium_wgl.dll']:
            shutil.copy2(mesa/name, native_test/name)
        clean = env.copy()
        clean['PATH'] = os.pathsep.join([str(Path(os.environ['SystemRoot'])/'System32'), os.environ['SystemRoot']])
        layout = work/'standalone-workspace.ini'
        run([native_test/'qupathxx_app.exe', '--project', work/'standalone-input.qpxproj', '--layout', layout, '--smoke'], work/'standalone-gui.log', clean, 120)
        if not layout.read_bytes().startswith(b'; qupathxx workspace layout v1\n') or b'[Docking][Data]' not in layout.read_bytes():
            raise RuntimeError('standalone docked workspace did not save its separate layout')
        run([native_test/'qupathxx_app.exe', '--project', work/'standalone-input.qpxproj', '--layout', layout, '--smoke'],
            work/'standalone-layout-reload.log', clean, 120)
        run([native_test/'qupathxx_batch.exe', work/'standalone-input.qpxproj', work/'standalone-batch', '--kind', 'stains'],
            work/'standalone-batch.log', clean, 120)
        if len(list((work/'standalone-batch').rglob('*.qpxproj'))) != 1:
            raise RuntimeError('standalone batch did not save its checkpoint')
        ppm = work/'standalone.ppm'
        ppm.write_bytes(b'P6\n2 2\n255\n'+bytes([16, 32, 64])*4)
        pam = work/'standalone.pam'
        run([native_test/'qupathxx_region.exe', ppm, '0', '0', '0', '2', '2', pam], work/'standalone-region.log', clean, 60)
        if pam.read_bytes().split(b'ENDHDR\n', 1)[1] != bytes([16, 32, 64, 255])*4:
            raise RuntimeError('standalone region pixels differ')
        thumb = work/'standalone-thumbnail.pam'
        run([native_test/'qupathxx_region.exe', '--thumbnail', ppm, '1', '1', thumb], work/'standalone-thumbnail.log', clean, 60)
        if thumb.read_bytes().split(b'ENDHDR\n', 1)[1] != bytes([16, 32, 64, 255]):
            raise RuntimeError('standalone thumbnail pixels differ')
        tiff = work/'standalone-thumbnail.tif'
        run([test_python, '-I', '-c',
             "import sys,numpy as np,tifffile; tifffile.imwrite(sys.argv[1],np.full((32,48,3),[40,80,160],dtype=np.uint8),photometric='rgb',tile=(16,16),compression='deflate',metadata=None)",
             tiff], work/'standalone-thumbnail-fixture.log', env, 60)
        tiff_thumb = work/'standalone-tiff-thumbnail.pam'
        run([native_test/'qupathxx_region.exe', '--thumbnail', tiff, '31', '23', tiff_thumb],
            work/'standalone-tiff-thumbnail.log', clean, 60)
        raw = tiff_thumb.read_bytes()
        if b'WIDTH 31\nHEIGHT 20\n' not in raw or raw.split(b'ENDHDR\n', 1)[1] != bytes([40, 80, 160, 255])*620:
            raise RuntimeError('standalone TIFF thumbnail pixels differ')
        slide = Path(env['QUPATHXX_APERIO_FIXTURE'])
        run([native_test/'qupathxx_app.exe', slide, '--smoke'], work/'standalone-slide-gui.log', clean, 120)
        slide_region = work/'standalone-slide.pam'
        run([native_test/'qupathxx_region.exe', slide, '0', '0', '0', '32', '24', slide_region],
            work/'standalone-slide-region.log', clean, 60)
        reference = work/'installed-slide.pam'
        run([install/'Lib/site-packages/qupathxx/qupathxx_region.exe', slide, '0', '0', '0', '32', '24', reference],
            work/'installed-slide-region.log', env, 60)
        if slide_region.read_bytes() != reference.read_bytes():
            raise RuntimeError('standalone slide pixels differ from installed reader')
        slide_thumb = work/'standalone-slide-thumbnail.pam'
        run([native_test/'qupathxx_region.exe', '--thumbnail', slide, '250', '150', slide_thumb],
            work/'standalone-slide-thumbnail.log', clean, 60, expected=1)
        if slide_thumb.exists() or 'source pixel limit' not in (work/'standalone-slide-thumbnail.log').read_text(encoding='utf8'):
            raise RuntimeError('standalone Aperio thumbnail must reject its inadequate default source budget before writing')
    (work/'result.json').write_text(json.dumps(dict(mode=args.mode, extended_readers=args.extended_readers, wheel=wheel.name, wheel_sha256=assets.sha(wheel),
                                                  python=sys.version, tested_utc=time.time()), indent=2)+'\n', encoding='utf8')


if __name__ == '__main__':
    main()
