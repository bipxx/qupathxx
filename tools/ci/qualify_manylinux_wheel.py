"""Audit every packaged ELF before allowing a manylinux platform tag."""
import argparse
import hashlib
import importlib.metadata
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import zipfile


def sha(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def requirement_errors(needed,versions,provided,whitelist,allowed_versions):
    errors=[]
    for name in needed:
        if name not in provided and name not in whitelist:errors.append('unbundled dependency: '+name)
    for library,required in versions.items():
        if library in whitelist:
            for version in required:
                if version not in allowed_versions:errors.append(library+': unsupported version '+version)
    return sorted(errors)


def scan_wheel(wheel,policy):
    from elftools.elf.elffile import ELFFile
    payloads={};provided=set()
    with zipfile.ZipFile(wheel) as archive:
        for name in archive.namelist():
            if name.endswith('/'):continue
            data=archive.read(name)
            if not data.startswith(b'\x7fELF'):continue
            elf=ELFFile(io.BytesIO(data))
            if elf.elfclass!=64 or not elf.little_endian or elf['e_machine']!='EM_X86_64':
                raise ValueError('unsupported ELF architecture: '+name)
            needed=[];soname=None
            dynamic=elf.get_section_by_name('.dynamic')
            if dynamic is not None:
                for tag in dynamic.iter_tags():
                    if tag.entry.d_tag=='DT_NEEDED':needed.append(tag.needed)
                    elif tag.entry.d_tag=='DT_SONAME':soname=tag.soname
            versions={};section=elf.get_section_by_name('.gnu.version_r')
            if section is not None:
                for library,requirements in section.iter_versions():
                    versions[library.name]=sorted({item.name for item in requirements})
            provided.add(Path(name).name)
            if soname:provided.add(soname)
            payloads[name]=dict(sha256=hashlib.sha256(data).hexdigest(),needed=needed,soname=soname,version_requirements=versions)
    expected={'qupathxx/qupathxx_region','qupathxx/qupathxx_batch','qupathxx/libgeos_c.so.1','qupathxx/libopenslide.so.1'}
    if not expected.issubset(payloads) or not any(n.startswith('qupathxx/_native.') for n in payloads) or not any(Path(n).name.startswith('libgeos.so.') for n in payloads):
        raise ValueError('full headless wheel must contain bindings, both CLIs and all bundled runtimes')
    allowed={namespace+'_'+version for namespace,versions in policy.symbol_versions.items() for version in versions}
    errors={}
    for name,entry in payloads.items():
        bad=requirement_errors(entry['needed'],entry['version_requirements'],provided,policy.whitelist,allowed)
        if bad:errors[name]=bad
    return payloads,errors


def runtime_payloads(wheel):
    with zipfile.ZipFile(wheel) as archive:
        return {name:hashlib.sha256(archive.read(name)).hexdigest() for name in archive.namelist()
                if not name.endswith('/') and '.dist-info/' not in name}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wheel',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--report',type=Path,required=True)
    args=parser.parse_args()
    assets=json.loads(Path(__file__).with_name('manylinux-assets.json').read_text(encoding='utf-8'))
    for name in ['auditwheel','patchelf','packaging','pyelftools']:
        if importlib.metadata.version(name)!=assets[name]:raise RuntimeError('qualification tool version differs: '+name)
    from auditwheel.policy import WheelPolicies
    from auditwheel.architecture import Architecture
    from auditwheel.libc import Libc
    policy=WheelPolicies(libc=Libc.GLIBC,arch=Architecture.x86_64).get_policy_by_name(assets['policy'])
    if args.report.resolve()==args.wheel.resolve():parser.error('report must not overwrite input wheel')
    args.report.parent.mkdir(parents=True,exist_ok=True)
    payloads,errors=scan_wheel(args.wheel,policy)
    report=dict(schema='qpx.manylinux-qualification/1',engine_commit=os.environ.get('GITHUB_SHA'),policy=policy.name,
                input_wheel=args.wheel.name,input_wheel_sha256=sha(args.wheel),elf_payloads=payloads,abi_errors=errors,
                tools={name:assets[name] for name in ['auditwheel','patchelf','packaging','pyelftools']},qualified=False)
    def save():args.report.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    save()
    if errors:raise RuntimeError('wheel exceeds requested ABI policy; see '+str(args.report))
    args.output.mkdir(parents=True,exist_ok=True)
    if list(args.output.glob('*.whl')):raise RuntimeError('qualification output directory already contains wheels')
    env=os.environ.copy();env['PATH']=str(Path(sys.executable).parent)+os.pathsep+env.get('PATH','')
    def run(arguments,name):
        with (args.report.parent/name).open('w',encoding='utf-8') as stream:
            done=subprocess.run([sys.executable,'-m','auditwheel',*map(str,arguments)],env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=300)
        if done.returncode:raise RuntimeError('auditwheel failed; see '+name)
    run(['show',args.wheel],'auditwheel-before.log')
    run(['repair','--plat',policy.name,'--only-plat','--wheel-dir',args.output,args.wheel],'auditwheel-repair.log')
    repaired=list(args.output.glob('*.whl'))
    if len(repaired)!=1:raise RuntimeError('expected one qualified wheel')
    result=repaired[0]
    after,errors=scan_wheel(result,policy)
    if errors or after!=payloads:raise RuntimeError('repair changed ELF payloads or failed ABI policy')
    if runtime_payloads(result)!=runtime_payloads(args.wheel):raise RuntimeError('repair changed runtime/notices payloads')
    if sha(args.wheel)!=report['input_wheel_sha256']:raise RuntimeError('input wheel changed during qualification')
    from packaging.utils import parse_wheel_filename
    tags=parse_wheel_filename(result.name)[3]
    if {tag.platform for tag in tags}!={policy.name}:raise RuntimeError('qualified filename has unexpected platform tags')
    with zipfile.ZipFile(result) as archive:
        metadata=next(n for n in archive.namelist() if n.endswith('.dist-info/WHEEL'))
        advertised=[line.removeprefix('Tag: ') for line in archive.read(metadata).decode().splitlines() if line.startswith('Tag: ')]
        if set(advertised)!={str(tag) for tag in tags}:raise RuntimeError('wheel metadata tags differ from filename')
    run(['show',result],'auditwheel-after.log')
    report.update(qualified=True,qualified_wheel=result.name,qualified_wheel_sha256=sha(result),
                  runtime_payloads_unchanged=True,python_tags=sorted({tag.interpreter for tag in tags}),
                  runtime_payload_sha256=runtime_payloads(result))
    save();print(json.dumps(dict(qualified=result.name,sha256=report['qualified_wheel_sha256'],policy=policy.name),indent=2))


if __name__=='__main__':main()
