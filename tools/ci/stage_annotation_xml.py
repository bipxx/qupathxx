"""Exact optional native XML providers; development packaging, never product logic."""
import argparse
import hashlib
import json
from pathlib import Path
from stage_annotation_sources import Package, COMMON, ROOT

sha=lambda data:hashlib.sha256(data).hexdigest()
XML_COMMON=COMMON+('plugins/format-common/annotation_xml.hpp','plugins/format-common/annotation_xml_expat.cmake')
PROVIDERS={'org.qupathxx.asap-xml':'asap_xml','org.qupathxx.ome-roi':'ome_roi'}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['asap-module','asap-validation','ome-module','ome-validation','compiled-inputs',
                 'base-package','base-admission','output','reference-intake']:
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--write-inventory',action='store_true');a=p.parse_args();assert not a.output.exists(),'Preserve previous products'
    inventory_path=ROOT/'tests/fixtures/manifest.json';inv=json.loads(inventory_path.read_text(encoding='utf8'))
    pkg=Package(inv,'Native ASAP XML and shared bounded-XML OME candidates; unchanged A04 host/SDK/CLI/runtime stays separate, per-component source/license obligations retained','annotation_xml_plugins')
    admission=json.loads(a.base_admission.read_text());base=json.loads((a.base_package/'SOURCE.json').read_text());assert admission['admission_exit']==0
    compiled=json.loads(a.compiled_inputs.read_text());host=base['host_inputs']
    for n,h in host.items():assert sha((ROOT/n).read_bytes())==h==compiled['sources'][n],'Changed supposedly frozen host input: '+n
    def borrow(n):pkg.borrow(n,a.base_package,admission)
    for n in admission['members']:
        if n.startswith(('native/','bin/','lib/cmake/','licenses/','source/cpp/','source/third_party/',
                         'source/format-plugin-kit/','source/deps/expat/','source/examples/','source/references/')) or n=='source/LICENSE':borrow(n)
    assert sha(pkg.files['native/annotation_native.lib'])==base['native_overlay_sha256']
    assert sha(pkg.files['bin/qupathxx_format_import.exe'])==base['cli_sha256']
    assert sha(pkg.files['bin/annotation_plugin_consumer.exe'])==base['public_consumer_sha256']
    paths=set(XML_COMMON)
    for folder in PROVIDERS.values():paths.update(f.relative_to(ROOT).as_posix() for f in (ROOT/'plugins'/folder).rglob('*') if f.is_file())
    extra=['cpp/tests/asap_xml_tests.cpp','cpp/tests/ome_roi_tests.cpp','tools/ci/check_asap_xml.py','tools/ci/check_ome_roi.py',
           'tools/ci/check_imagej_roi.py','tools/ci/check_annotation_source_contract.py','tools/ci/check_format_family_plugins.py',
           'tools/ci/stage_annotation_xml.py','tools/ci/stage_annotation_sources.py','tools/ci/stage_annotation_geometry.py',
           'tools/ci/check_material_staging.py','docs/benchmarks/asap-xml/README.md']
    for n in sorted(paths|set(extra)):
        data=(ROOT/n).read_bytes()
        if n in compiled['sources'] and not n.endswith(('.md','.py')):assert compiled['sources'][n]==sha(data),'Changed compiled source '+n
        if 'source/'+n in pkg.files:
            assert pkg.files['source/'+n]==data,'Changed borrowed source: '+n
            continue
        pkg.add('source/'+n,data,n,kind='asset' if n.endswith(('.md','.json')) else 'code')
    # Exact author/license notices accompany the functional reference identity;
    # upstream application code, datasets and assets are not runtime/package inputs.
    reference=json.loads(a.reference_intake.read_text())
    for n in ['Copyright.txt','License.txt']:
        f=a.reference_intake.parent/'references/asap'/n;data=f.read_bytes();record=reference['members'][n];assert sha(data)==record['sha256']
        ident='asap-reference-GPL-2.0-or-later-'+sha(data)[:24]
        pkg.records.setdefault(ident,{'filename':n,'source':record['url'],'bytes':len(data),'sha256':sha(data),
            'material':{'id':ident,'category':'admitted_open','kind':'notice','license_id':'GPL-2.0-or-later',
                        'license_evidence':reference['upstream']+'/blob/'+reference['commit']+'/Copyright.txt',
                        'attribution':'Geert Litjens; GPL license text Free Software Foundation','source_ids':[]}})
        pkg._member('licenses/asap-reference/'+n,data,ident)
    providers={};entries=[]
    for module,validation_path,ident in [(a.asap_module,a.asap_validation,'org.qupathxx.asap-xml'),(a.ome_module,a.ome_validation,'org.qupathxx.ome-roi')]:
        data=module.read_bytes();validation=json.loads(validation_path.read_text());assert validation['passed'] and validation['artifacts'][str(module.resolve())]==sha(data)
        entry=json.loads((module.parent/'format-plugins.json').read_text())['plugins'][0]
        assert entry['provider_id']==ident and entry['module']['path']==module.name and entry['module']['sha256']==sha(data) and not entry['dependencies'] and entry['abi']==(3 if ident.endswith('asap-xml') else 4)
        selected=list(XML_COMMON)+[n for n in paths if n.startswith('plugins/'+PROVIDERS[ident]+'/')]
        inputs={n:sha((ROOT/n).read_bytes()) for n in selected}
        inputs.update({n[len('source/'):]:sha(d) for n,d in pkg.files.items() if n.startswith(('source/format-plugin-kit/','source/deps/expat/'))})
        parents=[pkg.members['source/'+n] for n in selected]+[pkg.members[n] for n in pkg.members if n.startswith('source/deps/expat/')]
        pkg.add(module.name,data,'Qualified independent native XML provider '+ident,parents)
        providers[ident]={'module_sha256':sha(data),'source_context_sha256':sha(json.dumps(inputs,sort_keys=True,separators=(',',':')).encode()),
            'source_inputs':inputs,'license_id':'AGPL-3.0-only','dependencies':{'private_static_Expat2.10.0':'MIT; SipHash CC0-1.0'},
            'validation_sha256':sha(validation_path.read_bytes()),'scope':'New module/source identity; frozen previous deliveries remain separate'}
        entries.append(entry)
    pkg.add('format-plugins.json',(json.dumps({'schema':'qpx.format-plugins/1','plugins':entries},indent=2)+'\n').encode(),'Exact independently qualified XML provider catalog',kind='asset')
    notice='''Native ASAP parser, OME parser and shared bounded XML/value support: qupathxx contributors, AGPL-3.0-only. Matching corresponding source/root license included; AGPL obligations remain applicable.
Private unchanged Expat2.10.0: MIT except SipHash CC0-1.0. Exact sources and author/notices are preserved with their existing admitted material IDs. No DTD/entity fetch or external XML runtime.
ASAP XML is functionally referenced from upstream commit '''+reference['commit']+'''. Its author notice permits GPL version2 or later; exact Copyright.txt/License.txt and reference hashes/URLs retained. Upstream application/Qt/pugixml implementation code is not linked or bundled; no upstream image/dataset/test/model corpus used.
This work is derived in part from the OME specification. Copyright (C) 2002-2016 Open Microscopy Environment
OME core schema: CC BY4.0 (https://creativecommons.org/licenses/by/4.0/), exact unmodified schema/source/primary licensing references retained. This is a selected vector ROI implementation, not OME certification or an image decoder.
The A04 native service/CLI/public consumer and matching C++ source/headers/config are reused byte-for-byte. Their MIT JSON and public-domain SQLite interface headers/notices remain distinct. The FP04 SDK/core/runtime/source/notices and published wheels remain separate frozen products, not requalified/repackaged here. Windows compiler/CRT follow the exact recorded Release /MD profile.
'''
    pkg.add('NOTICE.txt',notice.encode(),'Per-component source/distribution obligations',kind='notice')
    pkg.add('README.md',b'''# Native annotation XML package

Read source/plugins/asap_xml/README.md and source/plugins/ome_roi/README.md.
ASAP has explicit float32/order/group/class/root-color semantics; OME keeps its
separate binary64/plane/image/lock contract. Unsupported source profiles refuse.
Both share bounded XML/QOX2 support; neither infers alignment or repairs data.
The two module/source identities are new. Native service/CLI/SDK/header/config
bytes are the exact A04 products over the separately retained FP04 base SDK.

Build either provider offline against source/format-plugin-kit and the exact
source/deps/expat selection using its README. No ASAP/Qt/Java/Python runtime or
network download. Installed public SDK example: source/examples/annotation_plugin,
CMAKE_PREFIX_PATH="<this package>;<frozen FP04 SDK>", Release /MD.
SOURCE.json binds per-provider/native identities; PACKAGE.json lists exact owned
members. Uninstall verifies owned hashes and inactive processes, preserving user
projects, fixtures, notes, frozen sources/runtime and durable evidence.
''','Install/build/removal and profile guide',kind='asset')
    metadata={'schema':'qpx.annotation-xml-delivery/1','providers':providers,'host_inputs':host,
        'native_overlay_sha256':base['native_overlay_sha256'],'cli_sha256':base['cli_sha256'],'public_consumer_sha256':base['public_consumer_sha256'],
        'base_sdk':base['base_sdk'],'host_changed':False,'native_base_delivery_source_sha256':sha((a.base_package/'SOURCE.json').read_bytes())}
    result=pkg.finish(a.output,'xml-providers-'+providers['org.qupathxx.asap-xml']['module_sha256'][:16],metadata,inventory_path,a.write_inventory)
    print(json.dumps({'files':result['files'],'admission_exit':result['admission_exit'],'output':str(a.output)}))


if __name__=='__main__':main()
