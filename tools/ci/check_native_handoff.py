"""Validate frozen native app/SDK installation without building the engine.

Development release check. Scientific interpretation/replay/export remains in
the installed C++ example, CLI and application. Removal is a separate checked
PowerShell step; this script never removes a directory or touches an old install.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import zipfile
import check_material_staging as admission


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


def files(root):
    return {p.relative_to(root).as_posix():sha(p) for p in sorted(root.rglob('*')) if p.is_file()}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True,help='Frozen L01 evidence root')
    parser.add_argument('--work',type=Path,required=True,help='New external per-user validation directory')
    parser.add_argument('--report',type=Path,required=True)
    args=parser.parse_args()
    if os.name!='nt':parser.error('This handoff is Windows x64 only')
    source,work,report=args.input.resolve(),args.work.absolute(),args.report.resolve()
    report.mkdir(parents=True,exist_ok=True)
    handoff=json.loads((source/'source-handoff.json').read_text(encoding='utf8'))
    frozen=source/'release-final/source'
    manifest=frozen/'tests/fixtures/manifest.json'
    if sha(manifest)!=handoff['canonical_inventory']['sha256']:
        raise ValueError('Frozen membership inventory changed')
    profile=json.loads(manifest.read_text(encoding='utf8'))['release_profiles']['windows-native']
    entries=admission.inventory([manifest,frozen/'assets/fonts/font-source.json',
        source/'release-final/generated-materials.json',source/'dependency-sources-final/generated-materials.json'])
    source_id=handoff['native_source_sha256']
    if profile['compiled_source_sha256']!=source_id:raise ValueError('Native source ID differs')
    archives={name:source/'release-final'/info['archive'] for name,info in handoff['archives'].items()}
    for name,path in archives.items():
        info=handoff['archives'][name]
        if sha(path)!=info['sha256'] or path.stat().st_size!=info['bytes']:
            raise ValueError('Frozen archive changed: '+name)
        admission.check_archive(path,entries)
    dependency=source/'dependency-sources-final'/handoff['dependency_companion']['archive']
    if sha(dependency)!=handoff['dependency_companion']['sha256']:
        raise ValueError('Dependency source companion changed')
    admission.check_archive(dependency,entries)
    # New prefix only, outside the checkout/cache and the old rejected TEMP tree.
    work.mkdir(parents=True,exist_ok=False)
    for part in (work,*work.parents):
        if admission.reparse(part):raise ValueError('Validation prefix contains a link/reparse point')
    native,sdk=work/'installed/native app',work/'installed/native sdk'
    for name,target in [('native',native),('sdk',sdk)]:
        target.mkdir(parents=True)
        with zipfile.ZipFile(archives[name]) as archive:archive.extractall(target)
        admission.check_stage(target,entries)
        for member in profile['stages'][name]:
            record=entries[member['material_id']]
            if sha(target/member['path'])!=record['sha256']:
                raise ValueError('Installed path identity changed: '+member['path'])
    installed={str(p):sha(p) for folder in [native,sdk] for p in folder.rglob('*') if p.is_file()}
    consumer,build=work/'consumer source',work/'consumer build'
    shutil.copytree(sdk/'share/qupathxx/examples/native_sdk',consumer)
    env=os.environ.copy()
    for key in list(env):
        if key.upper() in {'PYTHONPATH','PYTHONHOME','CMAKE_PREFIX_PATH','QUPATHXX_DIR','QUPATHXX_UI_LANGUAGE'}:
            env.pop(key)
    windows=Path(os.environ['SystemRoot'])
    env['PATH']=os.pathsep.join([str(windows/'System32'),str(windows)])
    cmake=Path('C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')
    commands=[]

    def run(name,command,cwd=work):
        command=[str(x) for x in command]
        commands.append({'name':name,'argv':command,'cwd':str(cwd)})
        with (report/(name+'.log')).open('wb') as log:
            result=subprocess.run(command,cwd=cwd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
        (report/'commands.json').write_text(json.dumps(commands,indent=2)+'\n',encoding='utf8')
        if result.returncode:raise RuntimeError(name+' failed; retained '+str(report/(name+'.log')))
        print(name+': passed',flush=True)

    run('sdk-configure',[cmake,'-S',consumer,'-B',build,'-G','Visual Studio 17 2022','-A','x64',
        '-Dqupathxx_DIR='+str(sdk/'lib/cmake/qupathxx'),'-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF',
        '-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF'])
    run('sdk-build',[cmake,'--build',build,'--config','Release','--parallel','2','--','/nr:false'])
    buildlog=(report/'sdk-build.log').read_text(encoding='utf8',errors='replace')
    include_paths=[Path(x.strip()).resolve() for x in re.findall(r'including file:\s*([^\r\n]+)',buildlog)]
    allowed=[sdk.resolve(),Path('C:/Program Files/Microsoft Visual Studio/2022/Community'),
             Path('C:/Program Files (x86)/Windows Kits'),Path('C:/Program Files/Windows Kits')]
    if not include_paths or any(not any(p.is_relative_to(a) for a in allowed) for p in include_paths):
        raise ValueError('Consumer includes escape installed SDK/Windows toolchain')
    if 'LNK4098' in buildlog:raise ValueError('Consumer CRT link conflict')
    for forbidden in ['C:/Users/HEQ/Projects/qupathxx/build/ci','C:/Users/HEQ/Projects/qupathxx/build/deps']:
        if forbidden.casefold() in buildlog.replace('\\','/').casefold():
            raise ValueError('Consumer refers to original source/dependency cache')
    user=work/'user data';user.mkdir()
    results=user/'native results'
    run('sdk-use',[build/'Release/sdk_consumer.exe',results],cwd=windows/'System32')
    result=json.loads((results/'consumer.json').read_text())
    if result['source_sha256']!=source_id or not result['saved_replay']:
        raise ValueError('Installed header/library or saved identity differs')
    project=results/'saved.qpx';before_project=sha(project)
    batch=native/'qupathxx_batch.exe'
    common=['--image','1','--record',result['de_record']]
    run('cli-verify-de',[batch,'--verify-expression-differential',project,*common,'--work-bytes','67108864'])
    run('cli-export-de',[batch,'--export-expression-differential',project,user/'genes.csv',*common])
    run('cli-export-replicates',[batch,'--export-expression-differential',project,user/'replicates.csv',*common,'--replicates'])
    for name in ['genes.csv','replicates.csv']:
        if (user/name).read_bytes()!=(results/name).read_bytes():raise ValueError('CLI/SDK export differs: '+name)
    common=['--image','1','--record',result['pca_id'],'--work-bytes','67108864']
    run('cli-replay-pca',[batch,'--replay-expression-reduction',project,*common])
    run('cli-plot-pca',[batch,'--export-expression-reduction',project,user/'embedding.svg',*common,'--plot'])
    # Authored RGB fixture and independent expected crop; no external images.
    pixels=bytes(range(36));image=user/'authored.ppm'
    image.write_bytes(b'P6\n4 3\n255\n'+pixels)
    run('region-crop',[native/'qupathxx_region.exe',image,'1','1','0','2','1',user/'crop.pam'])
    output=(user/'crop.pam').read_bytes().split(b'ENDHDR\n',1)
    expected=b''.join(pixels[(4+x)*3:(4+x+1)*3]+b'\xff' for x in [1,2])
    if len(output)!=2 or output[1]!=expected:raise ValueError('Native region samples differ')
    run('gui-project-open',[native/'qupathxx_app.exe','--project',project,'--layout',user/'preferences.ini','--smoke'])
    if sha(project)!=before_project:raise ValueError('Read-only GUI/CLI changed saved project')
    sentinel=native/'user scratch/research-notes.txt';sentinel.parent.mkdir()
    sentinel.write_text('Authored research notes must survive package removal.\n',encoding='utf8')
    protected={str(p):sha(p) for p in user.rglob('*') if p.is_file()}
    protected[str(sentinel)]=sha(sentinel)
    if any(sha(Path(p))!=digest for p,digest in installed.items()):raise ValueError('Installed package changed during use')
    receipt={'state':'install_and_use_passed','scope':'local Windows fresh per-user prefix; native only; no engine build',
        'source_sha256':source_id,'os':platform.platform(),'work':str(work),'native':str(native),'sdk':str(sdk),
        'installed_files':installed,'protected_user_files':protected,'consumer':result,
        'compiler_include_records':len(include_paths),'runtime_PATH':env['PATH'],
        'python_paths':'removed','engine_builds':0,'install_run_verified':True,'uninstall_verified':False}
    (report/'handoff.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8')
    print('Install/use passed; manifest-owned removal is still required.',flush=True)


if __name__=='__main__':main()
