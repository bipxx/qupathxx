"""Stage exact qualified COCO/native overlay materials with component terms.

Development packaging only. Reuses the frozen base SDK/source and the exact
delivered native mask objects; it does not recompile scientific code.
"""
import argparse
import hashlib
import json
from pathlib import Path
from stage_annotation_sources import COMMON, Package, ROOT


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['packet','base-package','base-admission','cache','output']:
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--write-inventory',action='store_true')
    a=parser.parse_args()
    assert not a.output.exists(),'Preserve previous products'
    inventory_path=ROOT/'tests/fixtures/manifest.json'
    inventory=json.loads(inventory_path.read_text(encoding='utf8'))
    pkg=Package(inventory,'Selected native COCO polygon/RLE instance masks, ABI5 and source-scoped native overlay; no COCO dataset/image/model material admitted','coco_annotation_plugin')
    admission=json.loads(a.base_admission.read_text());assert admission['admission_exit']==0
    base=json.loads((a.base_package/'SOURCE.json').read_text())
    validation=json.loads((a.packet/'validation-02/validation.json').read_text());assert validation['passed']
    compiled=json.loads((a.packet/'compiled-inputs.json').read_text())
    reused=json.loads((a.packet/'mask-object-reuse.json').read_text())
    current=set(compiled['sources'])
    # Borrow only exact public headers/config, dependency interface source and
    # notices. Replace changed native source/kit/product members explicitly.
    for name in admission['members']:
        borrow=(name.startswith(('lib/cmake/','source/third_party/','source/cpp/','source/examples/')) or
                name in ['source/LICENSE','licenses/json-LICENSE.MIT','licenses/sqlite-LICENSE.txt'])
        original=name.removeprefix('source/')
        if borrow and (not (ROOT/original).is_file() or (a.base_package/name).read_bytes()==(ROOT/original).read_bytes()):
            pkg.borrow(name,a.base_package,admission)

    def licensed(name,data,license_id,origin,parents=(),attribution=''):
        ident='coco-'+license_id+'-'+sha(data)[:24]
        pkg.records.setdefault(ident,{'filename':Path(name).name,'source':origin,'bytes':len(data),'sha256':sha(data),
            'material':{'id':ident,'category':'admitted_open','kind':'notice' if 'LICENSE' in name or 'COPYING' in name else 'code',
                        'license_id':license_id,'license_evidence':origin,'attribution':attribution,'source_ids':list(parents)}})
        pkg._member(name,data,ident)
        return ident

    reference=json.loads((a.packet/'reference-intake.json').read_text())
    for name,r in reference['members'].items():
        data=(a.packet/'references/cocoapi'/name).read_bytes();assert sha(data)==r['sha256'] and len(data)==r['bytes']
        licensed('source/references/cocoapi/'+name,data,'BSD-2-Clause',r['url'],attribution='Piotr Dollar and Tsung-Yi Lin; original notices retained')
    license_id=pkg.members['source/references/cocoapi/license.txt']
    licensed('licenses/coco/COCO-LICENSE.txt',(ROOT/'plugins/coco/COCO-LICENSE.txt').read_bytes(),'BSD-2-Clause',reference['members']['license.txt']['url'],[license_id],'Piotr Dollar and Tsung-Yi Lin')
    for name in ['build/deps/geos-sdk/include/geos_c.h','build/deps/geos-sdk/include/geos/export.h']:
        data=(ROOT/name).read_bytes()
        licensed('source/third_party/native-overlay/geos/'+name.split('/include/')[1],data,'LGPL-2.1-or-later',
                 'Exact admitted GEOS3.15.0 public header; release-src-geos inventory/source archive',['release-src-geos'],'GEOS upstream authors; original notices retained')
    licensed('licenses/geos/COPYING',(ROOT/'build/format-family-plugins/sdk/share/qupathxx/licenses/geos/COPYING').read_bytes(),
             'LGPL-2.1-or-later','GEOS3.15.0 COPYING; release-src-geos',['release-src-geos'],'Free Software Foundation; GEOS upstream authors')
    sources=set(COMMON)|current|{'plugins/format-common/annotation_json.hpp','cpp/tests/coco_tests.cpp','cpp/tests/coco_reference.c',
        'cpp/tests/annotation_mask_test_module.cpp','cpp/tests/format_plugin_c_header_test.c','tools/ci/check_coco.py','tools/ci/stage_coco.py',
        'tools/ci/stage_annotation_sources.py','tools/ci/stage_annotation_geometry.py','tools/ci/check_material_staging.py',
        'tools/ci/check_format_family_plugins.py','tools/ci/check_annotation_source_contract.py','docs/benchmarks/coco/README.md',
        'examples/annotation_geometry_host/CMakeLists.txt','examples/annotation_plugin/main.cpp','examples/annotation_plugin/CMakeLists.txt'}
    sources.update(p.relative_to(ROOT).as_posix() for p in (ROOT/'plugins/coco').iterdir() if p.is_file())
    for name in sorted(sources):
        if name.startswith('build/'):
            continue
        data=(ROOT/name).read_bytes()
        if name in compiled['sources']:assert sha(data)==compiled['sources'][name],'Changed compiled input '+name
        target='source/'+name
        if target in pkg.files:
            assert pkg.files[target]==data,'Changed frozen borrowed source '+name
            continue
        if name in ['plugins/coco/coco_mask.hpp','plugins/coco/COCO-LICENSE.txt']:
            licensed(target,data,'BSD-2-Clause','COCO mask API adaptation; plugins/coco/reference-lock.json',
                     [license_id,pkg.members['source/references/cocoapi/common/maskApi.c']],
                     'Piotr Dollar and Tsung-Yi Lin; qupathxx contributors; notices retained')
        else:
            pkg.add(target,data,name,kind='asset' if name.endswith(('.md','.json')) else 'code')
    for p in sorted((a.packet/'kit').rglob('*')):
        if not p.is_file():continue
        target='source/format-plugin-kit/'+p.relative_to(a.packet/'kit').as_posix()
        pkg.add(target,p.read_bytes(),'Matching ABI5 C kit '+p.relative_to(a.packet/'kit').as_posix(),kind='notice' if p.name=='LICENSE' else 'code')
    objects=[]
    for row in reused['records']:
        p=a.packet/'inputs/native'/(row['name']+'.obj');data=p.read_bytes();assert sha(data)==row['object_sha256']
        parents=[]
        for source,h in row['inputs'].items():
            relative=Path(source).relative_to(ROOT).as_posix();assert sha((ROOT/relative).read_bytes())==h
            name='source/'+relative
            if relative.startswith('build/deps/geos-sdk/include/'):
                name='source/third_party/native-overlay/geos/'+relative.split('/include/')[1]
            parents.append(pkg.members[name])
        objects.append(pkg.add('native/mask-objects/'+p.name,data,'Exact retained existing native mask authority '+row['source_receipt'],parents))
    host_parents=objects+[pkg.members['source/'+n] for n in compiled['host_units']]
    native=a.cache/'native/Release/qupathxx_annotation_geometry_native.lib'
    assert sha(native.read_bytes())==compiled['products']['native_overlay_sha256']
    native_id=pkg.add('native/annotation_native.lib',native.read_bytes(),'New source-scoped ABI5 host over unchanged FP04 core',host_parents)
    for source,name in [(a.cache/'native/Release/qupathxx_format_import.exe','qupathxx_format_import.exe'),(a.cache/'Release/annotation_plugin_consumer.exe','annotation_plugin_consumer.exe')]:
        assert sha(source.read_bytes())==compiled['products'][name]
        pkg.add('bin/'+name,source.read_bytes(),'Thin native consumer over qualified ABI5 host',[native_id])
    module=a.cache/'coco/Release/qpx_coco_plugin.dll';data=module.read_bytes()
    assert sha(data)==validation['artifacts'][str(module.resolve())]
    entry=json.loads((module.parent/'format-plugins.json').read_text())['plugins'][0]
    assert entry['provider_id']=='org.qupathxx.coco' and entry['abi']==5 and entry['module']['sha256']==sha(data) and not entry['dependencies']
    module_sources={n:compiled['sources'][n] for n in compiled['module_units']}
    module_sources.update({n.removeprefix('source/'):sha(v) for n,v in pkg.files.items() if n.startswith('source/format-plugin-kit/')})
    pkg.add(module.name,data,'Qualified independent optional native COCO provider',[pkg.members['source/'+n] for n in compiled['module_units']])
    providers={entry['provider_id']:{'module_sha256':sha(data),'source_context_sha256':sha(json.dumps(module_sources,sort_keys=True,separators=(',',':')).encode()),
        'source_inputs':module_sources,'licenses':['AGPL-3.0-only','BSD-2-Clause'],'dependencies':{},'validation_sha256':sha((a.packet/'validation-02/validation.json').read_bytes())}}
    pkg.add('format-plugins.json',(json.dumps({'schema':'qpx.format-plugins/1','plugins':[entry]},indent=2)+'\n').encode(),'Exact optional COCO provider manifest',kind='asset')
    notice='''COCO plugin/application/shared value/JSON/host code: qupathxx contributors, AGPL-3.0-only; matching corresponding sources and full root license included. Applicable AGPL obligations remain in force.
Bounded COCO mask port and pinned original code/reference sources: BSD-2-Clause, Piotr Dollar and Tsung-Yi Lin (2014/2015); qupathxx contributors' adaptation. Exact source and binary notices are retained under source/plugins/coco and licenses/coco. No COCO dataset/image/annotation/model asset or derivative is admitted by those code terms.
Existing native mask authority objects are reused exactly from their recorded later source receipts; matching source/private headers and original object identities are included. GEOS3.15.0 interface headers retain LGPL-2.1-or-later and original notices/COPYING; exact GEOS source archive release-src-geos remains separately retained with the frozen SDK. Native JSON3.12.0 MIT and SQLite3.54.1 public-domain interfaces/notices remain distinct borrowed materials.
The FP04 SDK/core/runtime, full native source archive and third-party source/notices are separate frozen required products with their existing identities and terms. They are not rebuilt or requalified here. The new annotation host/native CLI and public consumer have their own source/binary identity. Microsoft compiler/CRT follow the recorded Release /MD profile.
'''
    pkg.add('NOTICE.txt',notice.encode(),'Actual per-component source/binary/distribution obligations',kind='notice')
    pkg.add('README.md',b'''# Optional native COCO instance annotations

Read source/plugins/coco/README.md for the selected polygon/RLE, image/category/
instance/pixel/frame/class/crowd contract and CLI example. Matching module, C ABI5
kit, native overlay, thin CLI/public consumer, source and component notices are
included. The frozen FP04 SDK/runtime/source is a separate required product.
Build the COCO module offline against source/format-plugin-kit, without SDK,
Python, Java or network discovery. Installed SDK consumer: source/examples/
annotation_plugin, CMAKE_PREFIX_PATH="<this package>;<frozen FP04 SDK>", Release.
Native host source rebuild: source/examples/annotation_geometry_host with exact
native/mask-objects or supplied matching GEOS3.15.0 header. SOURCE.json binds the
actual primary source/products; offline products retain their separate evidence.
PACKAGE.json owns exact files. Removal verifies owned hashes and inactive
processes, preserving user projects, inputs, notes, frozen source and runtime.
''','Install/build/removal and support scope',kind='asset')
    metadata={'schema':'qpx.coco-delivery/1','providers':providers,'host_inputs':{n:compiled['sources'][n] for n in compiled['host_units']},
        'native_overlay_sha256':compiled['products']['native_overlay_sha256'],'cli_sha256':compiled['products']['qupathxx_format_import.exe'],
        'public_consumer_sha256':compiled['products']['annotation_plugin_consumer.exe'],'base_sdk':base['base_sdk'],
        'native_mask_reuse':reused,'native_source_context_sha256':sha(json.dumps(compiled['sources'],sort_keys=True,separators=(',',':')).encode()),
        'compiled_inputs_sha256':sha((a.packet/'compiled-inputs.json').read_bytes()),'host_changed':True}
    result=pkg.finish(a.output,'coco-'+providers[entry['provider_id']]['module_sha256'][:16],metadata,inventory_path,a.write_inventory)
    print(json.dumps({'files':result['files'],'admission_exit':result['admission_exit'],'output':str(a.output)}))


if __name__=='__main__':
    main()
