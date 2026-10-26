"""Compile the original test TU only; inspect object and analysis-only link products."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--out',type=Path,required=True);args=parser.parse_args()
    if platform.system()!='Darwin' or platform.machine()!='arm64':raise RuntimeError('actual Darwin arm64 required')
    root=Path(__file__).resolve().parents[2];out=args.out.resolve();out.mkdir(parents=True,exist_ok=False)
    source=root/'cpp/tests/pixel_workflow_tests.cpp';original='4a72fd84b7726927285bf58120d288513c65eb4e'
    subprocess.run(['git','diff','--exit-code',original,'--','cpp','src','cmake','CMakeLists.txt','pyproject.toml','tests'],cwd=root,check=True)
    report=dict(schema='qpx.original-test-object-observation/1',source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),original_source=original,test_source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),commands=[],product_repair_established=False)
    def run(name,command,required=True):
        with (out/(name+'.log')).open('w',encoding='utf-8') as log:
            done=subprocess.run(list(map(str,command)),cwd=root,stdout=log,stderr=subprocess.STDOUT,timeout=360)
        report['commands'].append(dict(name=name,command=list(map(str,command)),returncode=done.returncode))
        if required and done.returncode:raise RuntimeError(name+' failed; evidence preserved')
    try:
        run('host',['sw_vers']);run('architecture',['uname','-m']);run('xcode',['xcodebuild','-version']);run('compiler',['xcrun','clang++','--version'])
        flags=['-O3','-DNDEBUG','-std=gnu++17','-arch','arm64','-I',root/'cpp/include']
        object_path=out/'original-test.o'
        run('original-compile',['xcrun','clang++',*flags,'-c',source,'-o',object_path])
        run('original-assembly',['xcrun','clang++',*flags,'-S',source,'-o',out/'original-test.s'])
        report['object_sha256_before']=hashlib.sha256(object_path.read_bytes()).hexdigest()
        for label,arguments in [('symbols',['--syms']),('relocations',['--reloc']),('disassembly',['--disassemble','--demangle'])]:
            run('object-'+label,['xcrun','llvm-objdump','--macho',*arguments,object_path],required=False)
        run('object-names',['xcrun','nm','-nm',object_path])
        for mode,extra in [('default',[]),('no-deduplicate',['-Wl,-no_deduplicate'])]:
            binary=out/(mode+'-analysis-only')
            # No core or SDK is rebuilt. Missing qpx symbols intentionally remain
            # dynamic lookups; these analysis products MUST NOT be executed.
            run(mode+'-link',['xcrun','clang++',*flags,*extra,'-Wl,-undefined,dynamic_lookup',object_path,'-o',binary])
            for label,arguments in [('unwind',['--unwind-info']),('fixups',['--dyld-info']),('disassembly',['--disassemble','--demangle'])]:
                run(mode+'-'+label,['xcrun','llvm-objdump','--macho',*arguments,binary],required=False)
            run(mode+'-names',['xcrun','nm','-nm',binary])
        report['object_sha256_after']=hashlib.sha256(object_path.read_bytes()).hexdigest()
        if report['object_sha256_before']!=report['object_sha256_after']:raise RuntimeError('link input changed')
        report['analysis_products_executed']=False
        report['scope']='Original test TU object/assembly and two analysis-only link modes. No core/SDK rebuild, execution of incomplete products, platform qualification or repair claim.'
    finally:
        (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()
