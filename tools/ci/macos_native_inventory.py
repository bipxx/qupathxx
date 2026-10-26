"""Capture/audit native macOS build-host files; no relocated-package claim."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import tempfile
import zipfile


def module(name, filename):
    spec=importlib.util.spec_from_file_location(name,Path(__file__).with_name(filename))
    result=importlib.util.module_from_spec(spec);exec(compile(Path(spec.origin).read_bytes(),spec.origin,"exec",dont_inherit=True),result.__dict__)
    return result


common=module('qpx_mac_inventory_common','runtime_manifest.py')
macho=module('qpx_mac_inventory_reader','macho_dependencies.py')
require=common.require
SCHEMA='qpx.macos-native-file-inventory/1'
OTOOL_SCOPE='Captured binary load names and packed dylib versions agree with otool -L; no resolution/relocation/ABI/signing acceptance.'
MAGICS=(b'\xcf\xfa\xed\xfe',b'\xca\xfe\xba\xbe',b'\xca\xfe\xba\xbf')


def name(value):
    require(isinstance(value,str) and value and all(part and part not in ('.','..') for part in value.split('/')),
            'noncanonical Mac inventory path')
    require(not any(ord(c)<32 or ord(c)==127 or c=='\\' for c in value),'invalid Mac inventory path')
    require(len(value.encode('utf-8'))<=32768,'Mac inventory name exceeds budget')
    return value


def inspect_archive(path, architecture):
    path=Path(path)
    require(architecture in ['arm64','x86_64'],'unsupported Mac inventory architecture')
    files,binaries,names=[],[],set()
    total=metadata=names_size=0
    with path.open('rb') as stream:
        before, path_before=common.version(os.fstat(stream.fileno())),common.version(path.stat())
        require(before[:4]==path_before[:4] and before[2]<=1024*1024*1024,'Mac archive handle/path or size differs')
        digest=common.file_hash(stream);stream.seek(0)
        with zipfile.ZipFile(stream) as archive:
            require(1<=len(archive.infolist())<=10000,'Mac inventory entry budget exceeded')
            for entry in archive.infolist():
                require(entry.orig_filename==entry.filename and not entry.is_dir(),'normalized or directory Mac inventory entry')
                key=name(entry.filename)
                require(key not in names,'duplicate Mac inventory entry');names.add(key)
                names_size+=len(key.encode('utf-8'))
                require(names_size<=4*1024*1024,'Mac inventory names exceed budget')
                mode=entry.external_attr>>16
                require(stat.S_IFMT(mode)==stat.S_IFREG and not entry.flag_bits&1,'nonregular/encrypted Mac inventory entry')
                require(entry.file_size<=256*1024*1024 and total+entry.file_size<=512*1024*1024,'Mac inventory file/total byte budget exceeded')
                with archive.open(entry) as content:data=content.read(256*1024*1024+1)
                require(len(data)==entry.file_size,'Mac inventory file length differs')
                total+=len(data)
                files.append(dict(path=key,bytes=len(data),mode=mode&0o7777,sha256=hashlib.sha256(data).hexdigest()))
                native=key in ['native/qupathxx_batch','native/qupathxx_region']
                if native or key.endswith('.dylib') or data[:4] in MAGICS:
                    observed=macho.inspect_macho(data)
                    require(len(observed['slices'])==1 and observed['slices'][0]['machine']==architecture,'Mac inventory binary architecture differs')
                    require(observed['slices'][0]['kind']==('executable' if native else 'dylib'),'Mac inventory binary kind differs')
                    require(not native or mode&0o111,'Mac inventory native CLI lacks executable mode')
                    metadata+=len(common.canonical_json(observed))
                    require(metadata<=32*1024*1024,'Mac inventory binary metadata exceeds budget')
                    binaries.append(dict(path=key,**observed))
        require(common.file_hash(stream)==digest and common.version(os.fstat(stream.fileno()))==before
                and common.version(path.stat())==path_before,'Mac inventory archive changed during inspection')
    require({'native/qupathxx_batch','native/qupathxx_region'}<=names,'missing Mac native entry point')
    for key in names:
        require(all(str(parent) not in names for parent in PurePosixPath(key).parents),'Mac inventory parent is also file')
        require(key in ['native/qupathxx_batch','native/qupathxx_region'] or key.startswith(('sdk/geos/','sdk/opencv/')),'unexpected Mac inventory layout')
    for sdk in ['geos','opencv']:
        require(any(key.startswith('sdk/'+sdk+'/') and '/licenses/' in key for key in names),'Mac inventory omits SDK notices')
    return dict(schema=SCHEMA,archive=path.name,archive_sha256=digest,architecture=architecture,
                files=sorted(files,key=lambda item:item['path']),binaries=sorted(binaries,key=lambda item:item['path']),
                total_bytes=total,sdk_paths_may_be_embedded=True,runtime_resolution_complete=False,
                native_cli_modes_normalized=True,relocated_package_validated=False,external_runtime_availability_verified=False,
                dynamic_loading_exhaustively_verified=False,signing_verified=False,release_ready=False,
                scope='Complete native CLI and selected SDK runtime/notices snapshot file hashes/modes and Mach-O records. '
                      'SDK symlink aliases are regular byte copies; native CLI snapshot modes normalize to 0755. Build-host SDK paths may remain embedded. '
                      'No static archive/build-input inventory, installed-name resolution, relocation, dyld/ABI, signing or supported package acceptance.')


def capture(native, sdks, architecture, archive_path, output):
    native,archive_path,output=Path(native),Path(archive_path),Path(output)
    require(set(sdks)=={'geos','opencv'},'require both SDK roots')
    require(not archive_path.exists() and not output.exists() and archive_path.resolve()!=output.resolve(),'Mac inventory outputs must be new and distinct')
    selected={}
    for key in ['qupathxx_batch','qupathxx_region']:
        selected['native/'+key]=(native/key,native.resolve())
    for sdk,value in sdks.items():
        base=Path(value).resolve()
        for path in sorted(base.rglob('*')):
            if path.is_file() and (path.suffix=='.dylib' or '/licenses/' in path.as_posix()):
                selected['sdk/'+sdk+'/'+path.relative_to(base).as_posix()]=(path,base)
    require(len(selected)<=10000,'Mac capture file budget exceeded')
    archive_path.parent.mkdir(parents=True,exist_ok=True)
    temporary=None;total=0
    try:
        with tempfile.NamedTemporaryFile(dir=archive_path.parent,prefix='.'+archive_path.name+'.qpx-tmp-',delete=False) as handle:
            temporary=Path(handle.name)
        with zipfile.ZipFile(temporary,'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for key,(path,base) in sorted(selected.items()):
                require(path.resolve().is_relative_to(base),'SDK/native capture symlink escapes root')
                with path.open('rb') as stream:
                    info=os.fstat(stream.fileno())
                    source_mode=info.st_mode&0o7777
                    before,path_before=common.version(info),common.version(path.stat())
                    require(before[:4]==path_before[:4] and before[2]<=256*1024*1024,'Mac capture file handle/path or size differs')
                    data=stream.read(256*1024*1024+1)
                    require(len(data)==before[2] and common.version(os.fstat(stream.fileno()))==before
                            and common.version(path.stat())==path_before,'Mac capture source changed')
                total+=len(data);require(total<=512*1024*1024,'Mac capture total byte budget exceeded')
                entry=zipfile.ZipInfo(name(key));entry.create_system=3
                entry.external_attr=(stat.S_IFREG | (0o755 if key.startswith('native/') else source_mode))<<16
                entry.compress_type=zipfile.ZIP_DEFLATED
                archive.writestr(entry,data)
        # Validate before publishing the artifact. The manifest names the final
        # file, so inspect the same completed temporary bytes and rename its label.
        observed=inspect_archive(temporary,architecture)
        observed['archive']=archive_path.name
        with temporary.open('r+b') as stream:
            stream.flush();os.fsync(stream.fileno())
        os.link(temporary,archive_path)
        output.parent.mkdir(parents=True,exist_ok=True)
        common.publish_new(output,common.canonical_json(observed))
        return observed,{key:path for key,(path,base) in selected.items()}
    finally:
        if temporary is not None:temporary.unlink()


def parse_otool(text):
    records=[]
    pattern=re.compile(r'^\s+(.+) \(compatibility version (\d+)\.(\d+)\.(\d+), current version (\d+)\.(\d+)\.(\d+)\)(?: \(.*\))?$')
    for line in text.splitlines():
        if not line.strip() or (not line[0].isspace() and line.endswith(':')):continue
        match=pattern.fullmatch(line);require(match is not None,'unrecognized otool dependency output')
        parts=list(map(int,match.groups()[1:]))
        require(all(0<=parts[i]<=limit for i,limit in enumerate([65535,255,255]*2)),'otool version exceeds packed range')
        records.append(dict(name=match.group(1),compatibility_version=(parts[0]<<16)|(parts[1]<<8)|parts[2],
                            current_version=(parts[3]<<16)|(parts[4]<<8)|parts[5]))
    return records


def expected_links(binary):
    row=binary['slices'][0]
    entries=([row['dylib_id']] if row['dylib_id'] else [])+row['dependencies']
    return [dict(name=item['name'],compatibility_version=item['compatibility_version'],current_version=item['current_version']) for item in entries]


def link_key(item):return (item['name'],item['compatibility_version'],item['current_version'])


def otool_probe(manifest, sources, folder, output):
    folder=Path(folder);records=[]
    expected_hashes={item['path']:item['sha256'] for item in manifest['files']}
    require(not Path(output).exists(),'otool report must be new')
    for i,binary in enumerate(manifest['binaries']):
        key=binary['path'];log=folder/f'macho-otool-{i:04}.log'
        require(not log.exists(),'otool log must be new')
        path=sources[key]
        before=common.version(path.stat())
        with path.open('rb') as stream:require(common.file_hash(stream)==expected_hashes[key],'otool source differs from captured bytes')
        done=subprocess.run(['/usr/bin/otool','-arch',manifest['architecture'],'-L',str(path)],capture_output=True,timeout=60)
        with path.open('rb') as stream:require(common.file_hash(stream)==expected_hashes[key] and common.version(path.stat())==before,'otool source changed during probe')
        require(done.returncode==0,'otool failed on captured source')
        log.write_bytes(done.stdout)
        parsed=parse_otool(done.stdout.decode('utf-8'))
        require(sorted(parsed,key=link_key)==sorted(expected_links(binary),key=link_key),'Mach-O inventory differs from otool')
        records.append(dict(path=key,log=log.name,log_sha256=hashlib.sha256(done.stdout).hexdigest(),dependencies=parsed))
    report=dict(schema='qpx.macos-otool-probe/1',architecture=manifest['architecture'],archive_sha256=manifest['archive_sha256'],
                records=records,release_ready=False,scope=OTOOL_SCOPE)
    common.publish_new(Path(output),common.canonical_json(report))
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-build',type=Path,required=True)
    parser.add_argument('--geos-sdk',type=Path,required=True)
    parser.add_argument('--opencv-sdk',type=Path,required=True)
    parser.add_argument('--architecture',choices=['arm64','x86_64'],required=True)
    parser.add_argument('--archive',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--otool-output',type=Path,required=True)
    args=parser.parse_args()
    require(not args.otool_output.exists(),'otool output must be new')
    result,sources=capture(args.native_build,dict(geos=args.geos_sdk,opencv=args.opencv_sdk),args.architecture,args.archive,args.output)
    otool_probe(result,sources,args.output.parent,args.otool_output)
    print(json.dumps(dict(files=len(result['files']),binaries=len(result['binaries']),release_ready=False)))


if __name__=='__main__':main()
