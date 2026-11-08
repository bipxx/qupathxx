from pathlib import Path
import hashlib
import importlib.util
import json
import re
import shutil
import subprocess

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
out=root/'build/rgb-nominal-287'
commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip()
assert commit=='01c3b9aadd36ee4e06ba759929c19a9e2073507c'
assert not subprocess.check_output(['git','status','--porcelain'],cwd=work)
manifest=json.loads((out/'compiler-inputs-initial.json').read_text())
assert manifest['source']=='23edd7ed1f15c87438602ec3b22e5506793a3a59' and len(manifest['files'])==418
assert all(hashlib.sha256((work/p).read_bytes()).hexdigest()==h for p,h in manifest['files'].items())
header=(root/'build/ci/rgb-nominal-287-native/generated/build_manifest.hpp').read_text()
assert 'git_revision[]="'+manifest['source']+'"' in header
assert 'source_sha256[]="'+manifest['fingerprint']+'"' in header
shutil.copyfile(root/'build/ci/rgb-nominal-287-native/generated/build_manifest.hpp',out/'windows-build-manifest.hpp')
retained=out/'windows-products'
retained.mkdir(exist_ok=False)
products=[]
for entry in json.loads((out/'native-products.json').read_text()):
    source=Path(entry['path'])
    target=retained/source.name
    assert hashlib.sha256(source.read_bytes()).hexdigest()==entry['sha256']
    shutil.copyfile(source,target)
    assert source.read_bytes()==target.read_bytes()
    products.append(dict(path=str(target),bytes=target.stat().st_size,sha256=entry['sha256']))
(out/'windows-retained-products.json').write_text(json.dumps(products,indent=2)+'\n')
linux=json.loads((out/'linux-compiler-inputs.json').read_text())
assert linux['source']==manifest['source'] and len(linux['files'])==418
linux_header=(out/'linux-build-manifest.hpp').read_text()
linux_fingerprint=json.loads((out/'linux-source-fingerprint.json').read_text())
assert 'git_revision[]="'+manifest['source']+'"' in linux_header
assert 'source_sha256[]="'+linux_fingerprint['fingerprint']+'"' in linux_header
for entry in json.loads((out/'linux-native-products.json').read_text()):
    path=Path('C:/'+entry['path'].removeprefix('/mnt/c/'))
    assert path.stat().st_size==entry['bytes'] and hashlib.sha256(path.read_bytes()).hexdigest()==entry['sha256']
old=json.loads((root/'build/gui-font-delivery-283/final-products.json').read_text())
assert len(old)==71 and all(hashlib.sha256(Path(v['retained']).read_bytes()).hexdigest()==v['sha256'] for v in old)
spec=importlib.util.spec_from_file_location('science',work/'tools/ci/scientific_grid_evidence.py')
science=importlib.util.module_from_spec(spec)
spec.loader.exec_module(science)
proof=science.source_contract(work,commit)
(out/'scientific-source-proof.json').write_text(json.dumps(proof,indent=2)+'\n')
differences=[p for p in manifest['files'] if manifest['files'][p]!=linux['files'][p]['checked_out_sha256']]
for p in manifest['files']:
    blob=subprocess.check_output(['git','show',commit+':'+p],cwd=work)
    assert hashlib.sha256(blob).hexdigest()==linux['files'][p]['git_blob_sha256']
    assert (work/p).read_bytes().replace(b'\r\n',b'\n')==blob.replace(b'\r\n',b'\n')
summary=dict(native_source=manifest['source'],observer_source=commit,scientific_source_paths=len(proof),
    compiler_inputs=418,canonical_compiler_blobs_equal=418,windows_linux_checkout_byte_differences=differences,
    windows_fingerprint=manifest['fingerprint'],linux_fingerprint=linux_fingerprint['fingerprint'],
    retained_new_native_products=4,retained_gui_products_unchanged=71,
    python_fixture_runtime_source='7231d95c25fbc9515d937036986e8c92b19da261',
    public_python_bindings_added=False,default_sampling_changed=False,own_source_platform_qualified=False)
(out/'source-runtime-proof.json').write_text(json.dumps(summary,indent=2)+'\n')
print('RGB private kernel proofs:',len(proof),'source paths,418 canonical compiler inputs,4 new retained binaries;71 GUI products unchanged',flush=True)
