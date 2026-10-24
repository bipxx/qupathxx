"""Relocate trusted native Mac snapshot copies using Apple tools, then reinspect.

Ad-hoc signatures support builder execution only, not publisher authentication.
Sources and original snapshot bytes are never modified. Runtime probes are a
separate gate; static admission never sets release readiness.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import stat
import subprocess
import tempfile
import zipfile

spec=importlib.util.spec_from_file_location('qpx_mac_relocate_plan',Path(__file__).with_name('macos_relocation_plan.py'))
planner=importlib.util.module_from_spec(spec);spec.loader.exec_module(planner)
inventory=planner.inventory
common=inventory.common
require=inventory.require


def verify_binary(edit,data):
    actual=inventory.macho.inspect_macho(data)
    require(len(actual['slices'])==1,'relocated binary must have one native slice')
    row=actual['slices'][0];before=edit['original_metadata']
    for key in ['machine','cpu_type','cpu_subtype','kind','flags','minimum_os','uuid','interpreters',
                'interpreter_ids','dyld_environment','sub_frameworks','sub_umbrellas','sub_clients','sub_libraries']:
        require(row[key]==before[key],'relocation changed protected Mach-O metadata: '+key)
    require(row['dependencies']==edit['expected_dependencies'] and row['dylib_id']==edit['expected_identity'],
            'relocated dependency identity/kind/order/version differs from plan')
    require(row['rpaths']==[],'relocated binary retains RPATH')
    ignored={0x1d,0x8000001c} # signature command may change; source RPATHs vanish
    require([cmd for cmd in row['commands'] if cmd not in ignored]==[cmd for cmd in before['commands'] if cmd not in ignored],
            'relocation changed other load-command identities/order')
    return actual


def inspect_archive(path,plan):
    path=Path(path)
    expected={item['target']:item for item in plan['files']}
    edits={item['target']:item for item in plan['edits']}
    files,binaries,seen=[],[],set();total=0
    with path.open('rb') as stream:
        before,path_before=common.version(os.fstat(stream.fileno())),common.version(path.stat())
        require(before[:4]==path_before[:4] and before[2]<=1024*1024*1024,'relocated archive handle/path or byte budget differs')
        archive_hash=common.file_hash(stream);stream.seek(0)
        with zipfile.ZipFile(stream) as archive:
            require(len(archive.infolist())==len(expected)<=10000,'relocated archive file cohort differs')
            for entry in archive.infolist():
                require(entry.orig_filename==entry.filename and not entry.is_dir(),'normalized/directory relocated archive entry')
                key=inventory.name(entry.filename)
                require(key in expected and key not in seen,'unknown/duplicate relocated archive file');seen.add(key)
                mode=entry.external_attr>>16
                require(stat.S_IFMT(mode)==stat.S_IFREG and mode&0o7777==expected[key]['mode'] and not entry.flag_bits&1,
                        'relocated archive file type/mode differs')
                require(entry.file_size<=256*1024*1024 and total+entry.file_size<=512*1024*1024,'relocated archive expanded byte budget exceeded')
                with archive.open(entry) as content:data=content.read(256*1024*1024+1)
                require(len(data)==entry.file_size,'relocated archive entry size differs');total+=len(data)
                digest=hashlib.sha256(data).hexdigest()
                if key in edits:binaries.append(dict(path=key,**verify_binary(edits[key],data)))
                else:require(digest==expected[key]['source_sha256'],'relocation changed notice bytes')
                files.append(dict(path=key,bytes=len(data),mode=mode&0o7777,sha256=digest))
        require(common.file_hash(stream)==archive_hash and common.version(os.fstat(stream.fileno()))==before
                and common.version(path.stat())==path_before,'relocated archive changed during inspection')
    return dict(schema='qpx.macos-relocated-native-files/1',archive=path.name,archive_sha256=archive_hash,
                source_archive_sha256=plan['source_archive_sha256'],plan_sha256=hashlib.sha256(common.canonical_json(plan)).hexdigest(),
                architecture=plan['architecture'],files=sorted(files,key=lambda item:item['path']),
                binaries=sorted(binaries,key=lambda item:item['path']),total_bytes=total,
                static_loader_paths_match_plan=True,runtime_workflows_verified=False,
                external_runtime_availability_verified=False,symbol_abi_verified=False,
                signing_verified=False,release_ready=False,
                scope='Relocated archive bytes match direct loader-path plan and protected Mach-O metadata; notices preserved. '
                      'No runtime workflow, older-OS/ABI, publisher signature, installer or supported-release acceptance.')


def pack(stage,plan,output,verified_binary_hashes=None):
    stage,output=Path(stage),Path(output)
    require(not output.exists(),'relocated archive output must be new')
    expected={item['target'] for item in plan['files']}
    actual={path.relative_to(stage).as_posix() for path in stage.rglob('*') if path.is_file() or path.is_symlink()}
    require(actual==expected,'relocated stage file cohort differs')
    output.parent.mkdir(parents=True,exist_ok=True);temporary=None;total=0
    try:
        with tempfile.NamedTemporaryFile(dir=output.parent,prefix='.'+output.name+'.qpx-tmp-',delete=False) as stream:
            temporary=Path(stream.name)
        with zipfile.ZipFile(temporary,'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for item in plan['files']:
                path=stage/item['target']
                require(not path.is_symlink() and path.resolve().is_relative_to(stage.resolve()),'relocated stage file escapes root')
                require(path.stat().st_size<=256*1024*1024,'relocated stage file exceeds budget')
                info=zipfile.ZipInfo(item['target']);info.create_system=3
                info.external_attr=(stat.S_IFREG|item['mode'])<<16;info.compress_type=zipfile.ZIP_DEFLATED
                data=path.read_bytes();total+=len(data)
                require(total<=512*1024*1024,'relocated stage total byte budget exceeded')
                if verified_binary_hashes is not None and item['target'] in verified_binary_hashes:
                    require(hashlib.sha256(data).hexdigest()==verified_binary_hashes[item['target']],
                            'staged binary changed after signature verification')
                archive.writestr(info,data)
        result=inspect_archive(temporary,plan);result['archive']=output.name
        with temporary.open('r+b') as stream:stream.flush();os.fsync(stream.fileno())
        os.link(temporary,output)
        return result
    finally:
        if temporary is not None:temporary.unlink()


def clean_environment():
    return {key:value for key,value in os.environ.items() if not key.startswith(('DYLD_','LD_')) and key not in ['PYTHONPATH','PYTHONHOME']}


def apple_commands(edit,target):
    result=[];arguments=['/usr/bin/install_name_tool']
    if edit['install_id'] is not None and edit['install_id']!=edit['original_metadata']['dylib_id']['name']:
        arguments+=['-id',edit['install_id']]
    for old in edit['remove_rpaths']:arguments+=['-delete_rpath',old]
    for change in edit['changes']:arguments+=['-change',change['old'],change['new']]
    if len(arguments)>1:result.append(arguments+[target])
    result.append(['/usr/bin/codesign','--force','--sign','-','--timestamp=none',target])
    result.append(['/usr/bin/codesign','--verify','--strict',target])
    return result


def relocate(archive,architecture,source_roots,work):
    require(platform.system()=='Darwin' and platform.machine()==architecture,'Apple relocation tools require requested native macOS host')
    archive,work=Path(archive),Path(work)
    require(not work.exists(),'relocation work directory must be new')
    plan=planner.create_plan(archive,architecture,source_roots)
    work.mkdir(parents=True,exist_ok=False);stage=work/'candidate';stage.mkdir()
    common.publish_new(work/'relocation-plan.json',common.canonical_json(plan))
    with archive.open('rb') as stream:
        require(common.file_hash(stream)==plan['source_archive_sha256'],'relocation source archive differs from plan')
        stream.seek(0)
        with zipfile.ZipFile(stream) as source:
            total=0
            for item in plan['files']:
                info=source.getinfo(item['source'])
                require(info.file_size<=256*1024*1024 and total+info.file_size<=512*1024*1024,'relocation source payload exceeds byte budget')
                with source.open(info) as entry:data=entry.read(256*1024*1024+1)
                require(len(data)==info.file_size,'relocation source payload length differs');total+=len(data)
                require(hashlib.sha256(data).hexdigest()==item['source_sha256'],'relocation source payload differs from plan')
                target=stage/item['target'];target.parent.mkdir(parents=True,exist_ok=True)
                require(target.resolve().is_relative_to(stage.resolve()),'relocation target escapes stage')
                with target.open('xb') as output:output.write(data)
                target.chmod(item['mode'])
        require(common.file_hash(stream)==plan['source_archive_sha256'],'relocation source archive changed during staging')
    records=[]
    def run(arguments):
        done=subprocess.run(list(map(str,arguments)),env=clean_environment(),cwd=work,capture_output=True,timeout=120)
        stem=f'apple-tool-{len(records):04}'
        (work/(stem+'.stdout.log')).write_bytes(done.stdout)
        (work/(stem+'.stderr.log')).write_bytes(done.stderr)
        records.append(dict(arguments=list(map(str,arguments)),exit_code=done.returncode,
                            stdout_sha256=hashlib.sha256(done.stdout).hexdigest(),stderr_sha256=hashlib.sha256(done.stderr).hexdigest()))
        common.publish_new(work/(stem+'.json'),common.canonical_json(records[-1]))
        require(done.returncode==0,'Apple relocation/signature tool failed; retained '+stem+' logs')
    ordered=sorted(plan['edits'],key=lambda item:(item['target'].startswith('bin/'),item['target']))
    verified_hashes={}
    for edit in ordered:
        target=stage/edit['target'];commands=apple_commands(edit,target)
        for arguments in commands[:-1]:run(arguments)
        before_signature_verification=hashlib.sha256(target.read_bytes()).hexdigest()
        run(commands[-1])
        data=target.read_bytes();digest=hashlib.sha256(data).hexdigest()
        require(digest==before_signature_verification,'staged binary changed during signature verification')
        verify_binary(edit,data);verified_hashes[edit['target']]=digest
    del data
    result=pack(stage,plan,work/'macos-relocated-native.zip',verified_hashes)
    common.publish_new(work/'macos-relocated-file-inventory.json',common.canonical_json(result))
    with archive.open('rb') as stream:require(common.file_hash(stream)==plan['source_archive_sha256'],'original snapshot changed during relocation')
    report=dict(schema='qpx.macos-native-relocation-execution/1',architecture=architecture,
                source_archive_sha256=plan['source_archive_sha256'],relocated_archive_sha256=result['archive_sha256'],
                plan_sha256=result['plan_sha256'],apple_tools=records,verified_binary_sha256=verified_hashes,ad_hoc_integrity_verified_on_builder=True,
                native_runtime_workflows_verified=False,signing_verified=False,release_ready=False,
                scope='Apple install-name edits and ad-hoc signature integrity checks on staged copies, followed by archive reinspection. '
                      'Ad-hoc signing does not authenticate a publisher. Runtime workflows, distribution signing and supported release remain unverified.')
    common.publish_new(work/'relocation-execution.json',common.canonical_json(report))
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive',type=Path,required=True)
    parser.add_argument('--architecture',choices=['arm64','x86_64'],required=True)
    parser.add_argument('--source-roots',type=Path,required=True)
    parser.add_argument('--work',type=Path,required=True)
    args=parser.parse_args()
    require(args.source_roots.stat().st_size<=65536,'Mac source-root JSON exceeds budget')
    roots=json.loads(args.source_roots.read_text(encoding='utf-8'),object_pairs_hook=common.unique_object)
    print(json.dumps(relocate(args.archive,args.architecture,roots,args.work)))


if __name__=='__main__':main()
