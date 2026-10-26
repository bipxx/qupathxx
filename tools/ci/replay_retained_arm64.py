"""Reuse original diagnostic240 arm64 program/runtime; no product/SDK rebuild."""
import argparse
import hashlib
import json
import os
from pathlib import Path,PurePosixPath
import platform
import re
import shutil
import stat
import subprocess
import sys
import time
import zipfile

COMMIT='4a72fd84b7726927285bf58120d288513c65eb4e'
RUN=40384720525
ARTIFACT=12681332017
ARCHIVE_SHA='0b00dda78dcb791af3c173f834a5c8376478132d8f1312972639058f494677b9'
ARCHIVE_BYTES=9779737
PRODUCT_SHA='a55c203c8fc7c05c1dfddd5b6ee024c63a638f5c908b75f32a34cc1d8239b9a2'
PRODUCT_BYTES=8563968
SDK_SHA='9a7a1cf490e77dfb4478fdd81583ce6b8364303d51dbe42c748e3136da9bb892'

def require(condition,message):
    if not condition:raise RuntimeError(message)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def save(path,value):path.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')
def safe_extract(archive,destination,max_bytes=512*1024*1024):
    require(not destination.exists(),'extraction destination must be new')
    with zipfile.ZipFile(archive) as stream:
        items=stream.infolist();require(0<len(items)<=1024,'archive member limit');names=set();total=0
        for item in items:
            raw_name=item.orig_filename;path=PurePosixPath(raw_name);require(not path.is_absolute() and path.parts and not any(p in ('..','.') or ':' in p for p in path.parts) and chr(92) not in raw_name,'unsafe archive path')
            require(path.as_posix().casefold() not in names,'duplicate/case-colliding archive path');names.add(path.as_posix().casefold())
            mode=item.external_attr>>16;require(stat.S_IFMT(mode) in (0,stat.S_IFREG,stat.S_IFDIR),'archive special file');require(not item.flag_bits&1,'encrypted archive member')
            total+=item.file_size;require(total<=max_bytes and item.file_size<=128*1024*1024,'archive byte limit')
        destination.mkdir(parents=True)
        for item in items:
            target=destination/Path(*PurePosixPath(item.filename).parts)
            if item.is_dir():target.mkdir(parents=True,exist_ok=True)
            else:
                target.parent.mkdir(parents=True,exist_ok=True)
                with stream.open(item) as source,target.open('xb') as output:shutil.copyfileobj(source,output)

def one(root,name):
    found=list(root.rglob(name));require(len(found)==1,'ambiguous/missing '+name);return found[0]
def prepare(archive,out):
    require(archive.stat().st_size==ARCHIVE_BYTES and sha(archive)==ARCHIVE_SHA,'original artifact bytes/digest mismatch')
    unpacked=out/'original-artifact';safe_extract(archive,unpacked);sidecar=one(unpacked,'pixel-workflow-product.json');meta=json.loads(sidecar.read_text(encoding='utf-8'));product=sidecar.parent/'qupathxx_pixel_workflow_tests'
    require(meta['commit']==COMMIT and meta['architecture']=='arm64' and meta['path']==product.name,'original product source/architecture mismatch')
    require(meta['bytes']==PRODUCT_BYTES and meta['sha256']==PRODUCT_SHA and product.stat().st_size==PRODUCT_BYTES and sha(product)==PRODUCT_SHA,'original failing program identity mismatch')
    header=product.read_bytes()[:8];require(header[:4]==b'\xcf\xfa\xed\xfe' and int.from_bytes(header[4:8],'little')==0x100000c,'not original arm64 Mach-O')
    require(meta['flags']['QUPATHXX_BUILD_GEOS']=='ON' and meta['flags']['QUPATHXX_BUILD_CLASSIFIER_TRAINING']=='ON' and meta['flags']['QUPATHXX_BUILD_GUI']=='OFF' and meta['flags']['QUPATHXX_BUILD_PYTHON']=='OFF','unexpected original flags')
    inventory=one(unpacked,'macos-native-file-inventory.json');data=json.loads(inventory.read_text());sdk=inventory.parent/'macos-native-files.zip';require(data['architecture']=='arm64' and data['archive_sha256']==SDK_SHA and sha(sdk)==SDK_SHA,'retained runtime archive mismatch')
    runtime=out/'retained-runtime';safe_extract(sdk,runtime)
    for entry in data['files']:
        p=runtime/entry['path'];require(p.is_file() and p.stat().st_size==entry['bytes'] and sha(p)==entry['sha256'],'retained runtime member mismatch: '+entry['path']);p.chmod(entry['mode']&0o777)
    save(out/'original-identities.json',dict(runtime_source=COMMIT,original_run=RUN,original_artifact=ARTIFACT,archive_sha256=ARCHIVE_SHA,product=meta,runtime_archive_sha256=SDK_SHA,runtime_files=data['files'],scope='Authenticated retained inputs; diagnostic loader override only, no test/relocation/release acceptance.'))
    return product,runtime

def debug_commands(binary,script,library):
    def quote(value):return '"'+str(value).replace('\\','\\\\').replace('"','\\"')+'"'
    return '\n'.join(['settings set auto-confirm true','settings set target.disable-aslr false','target create '+quote(binary),'settings set target.env-vars '+quote('DYLD_LIBRARY_PATH='+str(library)),'command script import '+quote(script),'run','retained_summary'])+'\n'

def execute(out,name,args,env,cwd,timeout=240):
    start=time.monotonic();timed_out=False;code=None
    with (out/(name+'.log')).open('wb') as stream:
        try:code=subprocess.run(list(map(str,args)),stdout=stream,stderr=subprocess.STDOUT,env=env,cwd=cwd,timeout=timeout).returncode
        except subprocess.TimeoutExpired:timed_out=True
    data=dict(args=list(map(str,args)),cwd=str(cwd),returncode=code,timed_out=timed_out,seconds=time.monotonic()-start);save(out/(name+'-command.json'),data);require(not timed_out,name+' timed out; original log retained');return code

def debug(out,name,binary,script,library,env,cwd):
    local=env.copy();local['QPX_EXCEPTION_LOG']=str(out/(name+'-exceptions.jsonl'));local['QPX_EXCEPTION_SUMMARY']=str(out/(name+'-summary.json'))
    command=out/(name+'.lldb');command.write_text(debug_commands(binary,script,library),encoding='utf-8')
    code=execute(out,name,['xcrun','lldb','--batch','--source',command],local,cwd)
    require(code==0,name+' debugger did not complete');summary=json.loads((out/(name+'-summary.json')).read_text());require(summary['exited'],name+' inferior did not exit');require(not summary['observer_errors'],name+' observer error');require(all(bp['locations'] and bp['hits'] for bp in summary['breakpoints']),name+' breakpoints not exercised');return summary

def main(argv=None):
    parser=argparse.ArgumentParser();parser.add_argument('--archive',type=Path);parser.add_argument('--out',type=Path,required=True);parser.add_argument('--prepare-only',action='store_true');args=parser.parse_args(argv);out=args.out.resolve();out.mkdir(parents=True,exist_ok=False)
    archive=args.archive.resolve() if args.archive else out/'source-artifact.zip'
    if args.archive is None:
        metadata=json.loads(subprocess.check_output(['gh','api',f'repos/bipxx/qupathxx/actions/artifacts/{ARTIFACT}'],text=True));require(metadata['id']==ARTIFACT and metadata['name']=='macos-native-arm64' and not metadata['expired'] and metadata['workflow_run']['head_sha']==COMMIT and metadata['digest']=='sha256:'+ARCHIVE_SHA,'remote artifact metadata mismatch');save(out/'remote-artifact.json',{k:metadata[k] for k in ['id','name','size_in_bytes','digest','workflow_run','expired']})
        with archive.open('xb') as stream:subprocess.run(['gh','api',f'repos/bipxx/qupathxx/actions/artifacts/{ARTIFACT}/zip'],stdout=stream,check=True,timeout=60)
    product,runtime=prepare(archive,out)
    if args.prepare_only:return 0
    require(platform.system()=='Darwin' and platform.machine()=='arm64','original arm64 replay requires an arm64 Darwin host')
    root=Path(__file__).resolve().parents[2];subprocess.run(['git','-C',root,'diff','--exit-code',COMMIT,'--','cpp','src','cmake','CMakeLists.txt','pyproject.toml','tests'],check=True)
    env={k:v for k,v in os.environ.items() if not re.search('TOKEN|SECRET|PASSWORD|CREDENTIAL|PRIVATE_KEY',k,re.I)}
    library=runtime/'sdk/geos/lib';env['DYLD_LIBRARY_PATH']=str(library);cwd=root/'build/ci/macos-native/native-build';cwd.mkdir(parents=True,exist_ok=True);product.chmod(0o755)
    for name,command in [('host',['sw_vers']),('architecture',['uname','-a']),('xcode',['xcodebuild','-version']),('lldb-version',['xcrun','lldb','--version'])]:execute(out,name,command,env,cwd)
    plain=execute(out,'original-plain',[product],env,cwd);save(out/'plain-result.json',dict(runtime_source=COMMIT,program_sha256=PRODUCT_SHA,exit_status=plain))
    # The observer control compiles only this tiny exception witness, never the
    # retained product or any SDK. It admits memory decoding/catch association.
    probe=out/'exception-probe.cpp';probe.write_text('#include <stdexcept>\n#include <cstdio>\nint main(){try{throw std::invalid_argument("raw grid source series changed");}catch(const std::invalid_argument&){puts("typed probe caught");}try{throw std::runtime_error("wrong type probe");}catch(const std::exception&){puts("outer probe caught");}return 0;}\n',encoding='utf-8');probe_binary=out/'exception-probe'
    require(execute(out,'probe-build',['xcrun','clang++','-std=c++17','-O2',probe,'-o',probe_binary],env,cwd)==0,'observer probe compile failed')
    script=Path(__file__).with_name('lldb_retained_exceptions.py');probe_summary=debug(out,'probe',probe_binary,script,library,env,cwd);require(probe_summary['exit_status']==0 and probe_summary['throws']==2 and probe_summary['catches']==2,'probe exception census mismatch')
    events=[json.loads(line) for line in (out/'probe-exceptions.jsonl').read_text().splitlines()];require({e.get('message') for e in events if e['event']=='throw'}=={'raw grid source series changed','wrong type probe'} and all(e.get('matched_throw') for e in events if e['event']=='catch'),'probe memory/catch observer did not distinguish exceptions')
    traced=debug(out,'original-debugger',product,script,library,env,cwd)
    require(sha(product)==PRODUCT_SHA,'original program changed during replay')
    save(out/'result.json',dict(schema='qpx.retained-arm64-diagnostic/1',runtime_source=COMMIT,diagnostic_source=subprocess.check_output(['git','-C',root,'rev-parse','HEAD'],text=True).strip(),original_program_sha256=PRODUCT_SHA,plain_exit=plain,debugger_exit=traced['exit_status'],series_events=traced['series_events'],observer_probe_accepted=True,loader_override=str(library),scope='Unmodified retained binary/runtime replay and exception observation only. No runtime repair, T5 acceptance, relocation or release claim.'))
    print('Retained program replay observed:',plain,traced['exit_status'],len(traced['series_events']),'series throw/catch events',flush=True);return 0

if __name__=='__main__':raise SystemExit(main())
