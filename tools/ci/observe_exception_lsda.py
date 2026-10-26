"""Isolate arm64 exception-table behavior with the same objects and two link modes."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import sys


def classify(returncode, stdout):
    lines=stdout.splitlines()
    if len(lines)!=5 or lines[-2:]!=['wrong_type=escaped','missing=refused']:
        raise ValueError('witness negative controls or census differ')
    for name,line in zip(['invalid','length','runtime'],lines[:3]):
        if line!=name+'=expected' and not line.startswith(name+'=escaped:'):
            raise ValueError('witness positive identities differ')
    passed=all(line==name+'=expected' for name,line in zip(['invalid','length','runtime'],lines[:3]))
    if returncode!=(0 if passed else 1):
        raise ValueError('witness exit and positive cases disagree')
    return dict(expected_catches=passed,lines=lines,returncode=returncode)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--out',type=Path,required=True);args=parser.parse_args()
    if platform.system()!='Darwin' or platform.machine()!='arm64':raise RuntimeError('actual Darwin arm64 required')
    out=args.out.resolve();out.mkdir(parents=True,exist_ok=False)
    root=Path(__file__).resolve().parents[2];fixture=Path(__file__).with_name('fixtures')/'exception-lsda-v1'
    report=dict(schema='qpx.arm64-exception-link-observation/1',source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),commands=[],modes={},product_repair_established=False)
    def run(name,command,required=True):
        done=subprocess.run(list(map(str,command)),capture_output=True,text=True,timeout=120)
        (out/(name+'.log')).write_text(done.stdout+done.stderr,encoding='utf-8')
        report['commands'].append(dict(name=name,command=list(map(str,command)),returncode=done.returncode))
        if required and done.returncode:raise RuntimeError(name+' failed; evidence preserved')
        return done
    try:
        run('host',['sw_vers']);run('architecture',['uname','-m']);run('xcode',['xcodebuild','-version']);run('compiler',['xcrun','clang++','--version'])
        flags=['-O3','-DNDEBUG','-std=c++17','-arch','arm64']
        for name in ['main','throw']:
            source=fixture/(name+'.cpp');shutil.copyfile(source,out/source.name)
            run(name+'-compile',['xcrun','clang++',*flags,'-c',source,'-o',out/(name+'.o')])
            run(name+'-assembly',['xcrun','clang++',*flags,'-S',source,'-o',out/(name+'.s')])
        objects=[out/'main.o',out/'throw.o']
        report['objects_before']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in objects}
        for mode,extra in [('default',[]),('no-deduplicate',['-Wl,-no_deduplicate'])]:
            binary=out/mode
            run(mode+'-link',['xcrun','clang++',*flags,*extra,*objects,'-o',binary])
            result=run(mode+'-execute',[binary],required=False)
            report['modes'][mode]=classify(result.returncode,result.stdout)
            for label,arguments in [('unwind',['--unwind-info']),('fixups',['--dyld-info']),('disassembly',['--disassemble','--demangle'])]:
                run(mode+'-'+label,['xcrun','llvm-objdump','--macho',*arguments,binary],required=False)
        report['objects_after']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in objects}
        if report['objects_before']!=report['objects_after']:raise RuntimeError('link inputs changed')
        report['linker_difference_reproduced']=not report['modes']['default']['expected_catches'] and report['modes']['no-deduplicate']['expected_catches']
        report['scope']='Minimal standard-library witness only; no product, SDK, T5 or release repair is implied.'
    finally:
        (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()
