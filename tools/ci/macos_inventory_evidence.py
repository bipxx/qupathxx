"""Source-bound offline Mac native/SDK snapshot and otool artifact checks."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess

spec=importlib.util.spec_from_file_location('qpx_mac_snapshot_evidence',Path(__file__).with_name('macos_native_inventory.py'))
runtime=importlib.util.module_from_spec(spec);exec(compile(Path(spec.origin).read_bytes(),spec.origin,"exec",dont_inherit=True),runtime.__dict__)

SOURCES=['tools/ci/macos_native_inventory.py','tools/ci/macho_dependencies.py','tools/ci/runtime_manifest.py',
         'tools/ci/pe_dependencies.py','tools/ci/run_macos_native.py','tools/ci/macos_inventory_evidence.py',
         '.github/workflows/ci.yml']


def read(path):
    runtime.require(path.stat().st_size<=64*1024*1024,'Mac inventory evidence exceeds budget')
    return json.loads(path.read_text(encoding='utf-8'),object_pairs_hook=runtime.common.unique_object)


def audit(folder,root,commit,architecture,ci_result):
    folder,root=Path(folder),Path(root)
    sources={}
    for relative in SOURCES:
        committed=subprocess.run(['git','-C',str(root),'show',commit+':'+relative],capture_output=True)
        actual=(root/relative).read_bytes()
        runtime.require(committed.returncode==0 and actual==committed.stdout,'Mac inventory tooling differs from dispatched source: '+relative)
        sources[relative]=hashlib.sha256(actual).hexdigest()
    archive=folder/'macos-native-files.zip'
    manifest_path=folder/'macos-native-file-inventory.json'
    manifest=runtime.inspect_archive(archive,architecture)
    runtime.require(runtime.common.canonical_json(read(manifest_path))==runtime.common.canonical_json(manifest),'Mac inventory differs from uploaded archive')
    files={item['path']:item['sha256'] for item in manifest['files']}
    runtime.require(files['native/qupathxx_batch']==ci_result['native_batch_sha256'] and
                    files['native/qupathxx_region']==ci_result['native_region_sha256'],'Mac native snapshot differs from tested binaries')
    for sdk in ['geos','opencv']:
        expected={'sdk/'+sdk+'/'+key:digest for key,digest in ci_result['dependencies'][sdk].items()
                  if key.endswith('.dylib') or '/licenses/' in '/'+key}
        actual={key:digest for key,digest in files.items() if key.startswith('sdk/'+sdk+'/')}
        runtime.require(actual==expected,'Mac SDK runtime/notices snapshot differs from build ledger')
    probe_path=folder/'macos-otool-probe.json'
    probe=read(probe_path)
    runtime.require(set(probe)=={'schema','architecture','archive_sha256','records','release_ready','scope'} and
                    probe['scope']==runtime.OTOOL_SCOPE,'Mac otool evidence fields/scope differ')
    runtime.require(probe['schema']=='qpx.macos-otool-probe/1' and probe['architecture']==architecture
                    and probe['archive_sha256']==manifest['archive_sha256'] and probe['release_ready'] is False,'Mac otool source/scope differs')
    binaries={item['path']:item for item in manifest['binaries']}
    runtime.require(sorted(item['path'] for item in probe['records'])==sorted(binaries),'Mac otool binary cohort missing or duplicated')
    log_names=set();log_bytes=0
    for item in probe['records']:
        runtime.require(set(item)=={'path','log','log_sha256','dependencies'},'Mac otool record fields differ')
        log_name=item['log']
        runtime.require(re.fullmatch(r'macho-otool-[0-9]{4}\.log',log_name) is not None and log_name not in log_names,'invalid/duplicate otool log path')
        log_names.add(log_name)
        log=folder/log_name
        log_bytes+=log.stat().st_size
        runtime.require(log_bytes<=32*1024*1024,'Mac otool logs exceed aggregate budget')
        raw=log.read_bytes()
        runtime.require(hashlib.sha256(raw).hexdigest()==item['log_sha256'],'Mac otool log bytes changed')
        observed=runtime.parse_otool(raw.decode('utf-8'))
        runtime.require(runtime.common.canonical_json(observed)==runtime.common.canonical_json(item['dependencies']),'Mac otool parsed record differs')
        runtime.require(sorted(observed,key=runtime.link_key)==sorted(runtime.expected_links(binaries[item['path']]),key=runtime.link_key),
                        'Mac otool load names/versions differ from snapshot')
    return dict(source=commit,architecture=architecture,tooling_sha256=sources,files=len(files),binaries=len(binaries),
                archive_sha256=manifest['archive_sha256'],manifest_sha256=hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
                otool_probe_sha256=hashlib.sha256(probe_path.read_bytes()).hexdigest(),native_snapshot_accepted=True,
                sdk_paths_may_be_embedded=True,runtime_resolution_complete=False,relocated_package_validated=False,
                external_runtime_availability_verified=False,signing_verified=False,release_ready=False,
                scope='Uploaded native CLI and selected SDK runtime/notice bytes reinspected at exact source; independent otool names/versions agree. '
                      'Static SDK archives remain build-input ledger only. No install-name resolution, relocation, Python/GUI/OpenSlide, ABI, signing or supported release.')
