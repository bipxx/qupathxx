"""Validate the full native engine on macOS; no distributable package claim."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work',type=Path,required=True)
    parser.add_argument('--architecture',choices=['arm64','x86_64'],required=True)
    args=parser.parse_args()
    if platform.system()!='Darwin' or platform.machine()!=args.architecture:
        parser.error('requires the requested native macOS architecture')
    work=args.work.resolve();work.mkdir(parents=True,exist_ok=False)
    env=os.environ.copy()
    for key in ['PYTHONHOME','PYTHONPATH','LD_LIBRARY_PATH','LD_PRELOAD','DYLD_LIBRARY_PATH','DYLD_FALLBACK_LIBRARY_PATH','DYLD_INSERT_LIBRARIES']:
        env.pop(key,None)
    env['CMAKE_GENERATOR']='Ninja'
    def run(arguments,name,timeout=2400):
        print('Starting '+name,flush=True)
        with (work/(name+'.log')).open('w',encoding='utf-8') as stream:
            done=subprocess.run(list(map(str,arguments)),cwd=ROOT,env=env,
                stdout=stream,stderr=subprocess.STDOUT,timeout=timeout)
        if done.returncode:
            print((work/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-14000:],flush=True)
            raise RuntimeError(name+' failed')
        print('Passed '+name,flush=True)
    sys.path.insert(0,str(ROOT/'tools'))
    import build_opencv_sdk
    geos_work=work/'geos'
    run([sys.executable,'-I',ROOT/'tools/build_geos_sdk.py','--work-dir',geos_work,'--jobs','2'],'geos-build')
    opencv=work/'opencv-sdk'
    run([sys.executable,'-I',ROOT/'tools/build_opencv_sdk.py','--prefix',opencv,
         '--build-dir',work/'opencv-build','--source-dir',work/'sources','--jobs','2'],'opencv-build')
    flags=dict(CMAKE_BUILD_TYPE='Release',CMAKE_OSX_ARCHITECTURES=args.architecture,
        QUPATHXX_BUILD_TESTS='ON',QUPATHXX_BUILD_PYTHON='OFF',QUPATHXX_BUILD_GUI='OFF',
        QUPATHXX_BUILD_GEOS='ON',QUPATHXX_BUNDLE_GEOS='OFF',GEOS_ROOT=str(geos_work/'geos-sdk'),
        QUPATHXX_BUILD_OPENSLIDE='OFF',QUPATHXX_BUNDLE_OPENSLIDE='OFF',QUPATHXX_BUILD_TIFF='ON',
        QUPATHXX_BUILD_PROJECT='ON',QUPATHXX_BUILD_INTERCHANGE='ON',QUPATHXX_BUILD_CLASSIFIER_TRAINING='ON',
        OpenCV_DIR=str(build_opencv_sdk.cmake_config_directory(opencv)),
        QUPATHXX_OPENCV_NOTICES=str(opencv/'share/licenses/opencv'))
    build=work/'native-build'
    run(['cmake','-S',ROOT,'-B',build,*['-D'+key+'='+value for key,value in flags.items()]],'configure')
    run(['cmake','--build',build,'--config','Release','--parallel','2'],'build')
    run(['ctest','--test-dir',build,'-C','Release','--output-on-failure','--parallel','1',
         '--output-junit',work/'native-tests.xml'],'native-tests',1200)
    cases=list(ET.parse(work/'native-tests.xml').getroot().iter('testcase'))
    if len(cases)<43 or any(any(t.find(k) is not None for k in ['failure','error','skipped']) for t in cases):
        raise RuntimeError('full native test evidence is incomplete')
    names={t.get('name') for t in cases}
    required={'decimal_quantities','masked_gaussian','cells','geometry','project','classifier','pixel_features'}
    if not required.issubset(names):
        raise RuntimeError('native test evidence omits required engine modules: '+str(required-names))
    binary=build/'qupathxx_batch'
    run(['file',binary],'binary-architecture')
    if ('Mach-O 64-bit executable '+args.architecture) not in (work/'binary-architecture.log').read_text(encoding='utf-8'):
        raise RuntimeError('native batch architecture differs')
    run(['otool','-L',binary],'batch-runtime-links')
    source=work/'region.ppm';output=work/'region.pam'
    source.write_bytes(b'P6\n32 24\n255\n'+bytes([16,32,64])*768)
    run([build/'qupathxx_region',source,'0','0','0','32','24',output],'region-cli')
    if output.read_bytes().split(b'ENDHDR\n',1)[1]!=bytes([16,32,64,255])*768:
        raise RuntimeError('native region CLI pixels differ')
    dependencies={}
    for label,sdk in [('geos',geos_work/'geos-sdk'),('opencv',opencv)]:
        dependencies[label]={p.relative_to(sdk).as_posix():sha(p) for p in sorted(sdk.rglob('*'))
                             if p.is_file() and (p.suffix in ['.dylib','.a'] or '/licenses/' in p.as_posix())}
    report=dict(schema='qpx.macos-native-validation/1',commit=os.environ.get('GITHUB_SHA'),
        platform=platform.platform(),architecture=args.architecture,python=sys.version,
        compiler=subprocess.check_output(['c++','--version'],env=env,text=True).splitlines()[0],
        cmake=subprocess.check_output(['cmake','--version'],env=env,text=True).splitlines()[0],
        ninja=subprocess.check_output(['ninja','--version'],env=env,text=True).strip(),
        runner_image=dict(os=os.environ.get('ImageOS'),version=os.environ.get('ImageVersion')),
        flags=flags,tests=dict(passed=len(cases),names=sorted(names),junit_sha256=sha(work/'native-tests.xml')),
        dependencies=dependencies,native_batch_sha256=sha(binary),region_cli_exact=True,
        loader_overrides_used=False,python_gui_openslide_validated=False,release_ready=False,
        scope='Native engine/geometry/TIFF/project/interchange/classifier validation on the build host. SDK paths may be embedded. No relocated macOS wheel, OpenSlide, GUI, installer, signing or supported release acceptance.')
    (work/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(architecture=args.architecture,passed=len(cases),release_ready=False)))


if __name__=='__main__':
    main()
