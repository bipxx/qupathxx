"""Build, audit, retag and recheck a full headless wheel on the 2.28 baseline."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work',type=Path,required=True)
    parser.add_argument('--extended-readers',action='store_true')
    args=parser.parse_args()
    if platform.system()!='Linux' or platform.machine()!='x86_64' or os.confstr('CS_GNU_LIBC_VERSION')!='glibc 2.28':
        parser.error('run inside the pinned manylinux_2_28_x86_64 build image')
    root=Path(__file__).resolve().parents[2];work=args.work.resolve()
    assets=json.loads(Path(__file__).with_name('manylinux-assets.json').read_text(encoding='utf-8'))
    command=[sys.executable,'-I',str(Path(__file__).with_name('run.py')),'--mode','headless','--work',str(work)]
    if args.extended_readers:command+=['--extended-readers']
    subprocess.run(command,cwd=root,check=True)
    python=work/'install/bin/python';original=next((work/'wheels').glob('*.whl'))
    subprocess.run([sys.executable,'-I',str(Path(__file__).with_name('qualify_manylinux_wheel.py')),
                    '--wheel',str(original),'--output',str(work/'qualified'),'--report',str(work/'qualification.json')],cwd=root,check=True)
    qualified=next((work/'qualified').glob('*.whl'))
    clean=os.environ.copy()
    for name in ['LD_LIBRARY_PATH','LD_PRELOAD','PYTHONPATH','PYTHONHOME']:clean.pop(name,None)
    def run(arguments,log):
        with (work/log).open('w',encoding='utf-8') as stream:
            done=subprocess.run(list(map(str,arguments)),cwd=root,env=clean,stdout=stream,stderr=subprocess.STDOUT,timeout=300)
        if done.returncode:raise RuntimeError('qualified wheel check failed: '+log)
    run([python,'-m','pip','install','--force-reinstall','--no-deps',qualified],'qualified-install.log')
    sdk=root/'build/ci/deps/openslide-linux/openslide-bin-4.0.1.2-linux-x86_64'
    geos=root/'build/deps/geos-sdk'
    run([python,'-I',root/'tools/ci/check_linux_runtime.py','--wheel',qualified,'--geos-sdk',geos,'--openslide-sdk',sdk,
         '--output',work/'qualified-runtime.json'],'qualified-runtime.log')
    with (sdk/'lib/libopenslide.so.1').open('rb') as stream:reader_sha=hashlib.file_digest(stream,'sha256').hexdigest()
    run([python,'-I',root/'tools/ci/check_install.py','--wheel',qualified,'--openslide-version','4.0.1',
         '--openslide-runtime-sha256',reader_sha,'--output',work/'qualified-installed.json'],'qualified-installed.log')
    (work/'build-image.json').write_text(json.dumps(dict(image=assets['builder']['image'],libc=os.confstr('CS_GNU_LIBC_VERSION'),
        python=sys.version,engine_commit=os.environ.get('GITHUB_SHA')),indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
