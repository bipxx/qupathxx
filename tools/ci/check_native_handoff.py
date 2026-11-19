"""Validate frozen native app/SDK installation without building the engine.

Development release check. Scientific interpretation/replay/export remains in
the installed C++ example, CLI and application. Removal is a separate checked
PowerShell step; this script never removes a directory or touches an old install.
"""
import argparse
import ast
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import subprocess
import stat
import zipfile
import check_material_staging as admission


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


def files(root):
    return {p.relative_to(root).as_posix():sha(p) for p in sorted(root.rglob('*')) if p.is_file()}


def legacy_bundle(source):
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
    return source_id, archives, profile['stages'], entries


def published_bundle(source, expected_digest):
    # Pin the independently reviewed handoff receipt, then every archive it names.
    receipt = source/'handoff.json'
    if sha(receipt) != expected_digest:
        raise ValueError('Handoff receipt differs from the pinned SHA-256')
    handoff = json.loads(receipt.read_bytes())
    source_id = handoff['source_sha256']
    records = handoff['archives']
    if (handoff['schema'] != 'qpx.windows-native-local-handoff/1' or handoff['state'] != 'verified_local'
            or handoff['python_runtime'] is not False or not re.fullmatch(r'[0-9a-f]{64}', source_id)
            or set(records) != {'native','sdk','source','dependency_source','optional_providers'}):
        raise ValueError('Unsupported Windows native handoff receipt')
    archives = {}
    for role, record in records.items():
        name = record['filename']
        if not re.fullmatch(r'qupathxx-[a-z0-9.-]+\.zip', name) or name in {path.name for path in archives.values()}:
            raise ValueError('Invalid or duplicate handoff archive name')
        path = source/name
        if path.stat().st_size != record['bytes'] or sha(path) != record['sha256']:
            raise ValueError('Handoff archive differs from the pinned receipt: ' + role)
        with zipfile.ZipFile(path) as bundle:
            names = set()
            for member in bundle.infolist():
                raw = member.orig_filename
                logical = PurePosixPath(raw)
                kind = (member.external_attr >> 16) & 0o170000
                if (not logical.parts or logical.is_absolute() or raw != logical.as_posix()
                        or any(c in '\\:<>"|?*' or ord(c) < 32 for c in raw) or '..' in logical.parts
                        or any(p.casefold() in admission.RESTRICTED_PARTS for p in logical.parts)
                        or any(p.rstrip(' .') != p or Path(p).is_reserved() for p in logical.parts)
                        or raw.casefold() in names or member.is_dir() or kind not in (0, stat.S_IFREG)
                        or member.flag_bits & 1):
                    raise ValueError('Unsafe handoff archive member: ' + raw)
                if role == 'optional_providers' and not raw.startswith('providers/'):
                    raise ValueError('Provider archive member outside providers directory')
                names.add(raw.casefold())
            if len(names) != record['members']:
                raise ValueError('Handoff archive membership count differs: ' + role)
            if role in {'native','sdk','source'}:
                metadata = json.loads(bundle.read('RELEASE.json'))
                if metadata['stage'] != role or metadata['source_sha256'] != source_id:
                    raise ValueError('Handoff archive source identity differs: ' + role)
            if role == 'source' and hashlib.sha256(bundle.read('source-inputs.sha256')).hexdigest() != source_id:
                raise ValueError('Handoff source manifest differs')
            if role == 'dependency_source' and json.loads(bundle.read('sources.json'))['source_sha256'] != source_id:
                raise ValueError('Dependency source companion identity differs')
        archives[role] = path
    return source_id, archives, None, None


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True,help='Frozen Windows handoff directory')
    parser.add_argument('--handoff-sha256',help='Pinned SHA-256 of handoff.json for a five-archive handoff')
    parser.add_argument('--work',type=Path,required=True,help='New, short external validation directory')
    parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--cmake',type=Path,help='Location of cmake.exe if absent from PATH')
    args=parser.parse_args()
    if os.name!='nt':parser.error('This handoff is Windows x64 only')
    source,work,report=args.input.resolve(),args.work.absolute(),args.report.resolve()
    if args.handoff_sha256:
        source_id, archives, stages, entries = published_bundle(source, args.handoff_sha256.lower())
    else:
        source_id, archives, stages, entries = legacy_bundle(source)
    report.mkdir(parents=True,exist_ok=True)
    # New prefix only, outside the checkout/cache and the old rejected TEMP tree.
    work.mkdir(parents=True,exist_ok=False)
    for part in (work,*work.parents):
        if admission.reparse(part):raise ValueError('Validation prefix contains a link/reparse point')
    native,sdk=work/'installed/native app',work/'installed/native sdk'
    for name,target in [('native',native),('sdk',sdk)]:
        target.mkdir(parents=True)
        with zipfile.ZipFile(archives[name]) as archive:archive.extractall(target)
        if stages is not None:
            admission.check_stage(target,entries)
            for member in stages[name]:
                record=entries[member['material_id']]
                if sha(target/member['path'])!=record['sha256']:
                    raise ValueError('Installed path identity changed: '+member['path'])
    if args.handoff_sha256:
        with zipfile.ZipFile(archives['optional_providers']) as archive:
            archive.extractall(work/'installed')
    installed={str(p):sha(p) for p in (work/'installed').rglob('*') if p.is_file()}
    consumer,build=work/'example',work/'b'
    shutil.copytree(sdk/'share/qupathxx/examples/native_sdk',consumer)
    env=os.environ.copy()
    for key in list(env):
        if key.upper() in {'PYTHONPATH','PYTHONHOME','CMAKE_PREFIX_PATH','QUPATHXX_DIR','QUPATHXX_UI_LANGUAGE'}:
            env.pop(key)
    windows=Path(os.environ['SystemRoot'])
    env['PATH']=os.pathsep.join([str(windows/'System32'),str(windows)])
    cmake=args.cmake or shutil.which('cmake.exe') or Path('C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')
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
    cache=(build/'CMakeCache.txt').read_text(encoding='utf8')
    vs=re.search(r'^CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)$',cache,re.MULTILINE)
    if not vs or not vs[1].strip():raise ValueError('Missing Visual Studio generator instance')
    allowed=[sdk.resolve(),Path(vs[1].strip()).resolve(),Path('C:/Program Files (x86)/Windows Kits'),
             Path('C:/Program Files/Windows Kits')]
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
    if args.handoff_sha256:
        image=user/'samples.vti'
        image.write_text('''<VTKFile type="ImageData" version="1.0" byte_order="LittleEndian">
<ImageData WholeExtent="0 1 0 1 0 0" Origin="0 0 0" Spacing="1 1 1">
<Piece Extent="0 1 0 1 0 0"><PointData Scalars="Samples">
<DataArray type="UInt16" Name="Samples" format="ascii">1 257 4096 65535</DataArray>
</PointData></Piece></ImageData></VTKFile>''',encoding='utf8')
        raw=user/'samples.npy'
        manifest=work/'installed/providers/vti/reader-plugins.json'
        run('vti-raw',[native/'qupathxx_region.exe','--reader-plugins',manifest,'--reader','org.qupathxx.vti',
                       '--reader-options','{"point_array":"Samples"}','--raw',image,raw])
        data=raw.read_bytes()
        if data[:8]!=b'\x93NUMPY\x01\x00':raise ValueError('Unexpected VTI raw result encoding')
        offset=10+int.from_bytes(data[8:10],'little')
        header=ast.literal_eval(data[10:offset].decode('ascii'))
        expected=b''.join(v.to_bytes(2,'little') for v in (1,257,4096,65535))
        if (header != dict(descr='<u2',fortran_order=False,shape=(2,2,1)) or data[offset:]!=expected):
            raise ValueError('Installed VTI provider raw samples differ')
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
    if args.handoff_sha256:
        receipt['archive_handoff_sha256']=args.handoff_sha256.lower()
    (report/'handoff.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8')
    print('Install/use passed; manifest-owned removal is still required.',flush=True)


if __name__=='__main__':main()
