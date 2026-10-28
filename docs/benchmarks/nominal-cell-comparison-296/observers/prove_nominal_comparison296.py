from pathlib import Path
import hashlib
import importlib.util
import json
import subprocess
import xml.etree.ElementTree as ET
import qupathxx as q
import qupathxx._native as native

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/nominal-cell-comparison-296';old=root/'build/nominal-cell-frontends-294'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
observer=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip();assert observer.startswith('0c874848')
assert not subprocess.check_output(['git','status','--porcelain'],cwd=work)
manifest=json.loads((old/'compiler-inputs-initial.json').read_text(encoding='utf-8'));assert len(manifest['files'])==424
assert q.build_environment().git_revision==manifest['source'] and q.build_environment().source_tree_sha256==manifest['fingerprint']
assert all(digest(work/p)==h for p,h in manifest['files'].items())
merged={}
for name in ('raw_classifier_evidence','scientific_grid_evidence'):
    spec=importlib.util.spec_from_file_location(name,work/('tools/ci/'+name+'.py'));module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    proof=module.source_contract(work,observer);(out/(name+'-source-proof.json')).write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8')
    for p,v in proof.items():assert p not in merged or merged[p]==v;merged[p]=v
assert len(merged)==896
for p in ('tools/compare_qupath_nominal_cells.py','tests/test_qupath_nominal_cells_reference.py'):assert p in merged
native_git=json.loads((old/'raw_classifier_evidence-source-proof.json').read_text(encoding='utf-8'));native_git.update(json.loads((old/'scientific_grid_evidence-source-proof.json').read_text(encoding='utf-8')))
for p in manifest['files']:assert merged[p]['git_blob_sha256']==native_git[p]['git_blob_sha256']
products=json.loads((old/'final-products.json').read_text(encoding='utf-8'));assert len(products)==71
for p in products:
    assert digest(Path(p['retained']))==p['sha256']
    assert digest(root/'build/ci/gridfrontends167-package'/p['path'])==p['sha256']
installed=json.loads((old/'installed-proof.json').read_text(encoding='utf-8'));site=Path(q.__file__).parent.parent
for p in installed['members']:assert digest(site/p['path'])==p['sha256']
linux=json.loads((out/'linux/installed-proof.json').read_text(encoding='utf-8'));assert linux['native_source']=='a7eb78de75387bce2da7b3d59b7f0a2ab6126cee'
result=json.loads((root/'build/q295-all/Linux-headless-py3.12/result.json').read_text(encoding='utf-8'))
assert linux['wheel_sha256']==result['wheel_sha256']
counts={}
for filename,expected in [('api.xml',(35,0)),('linux/api.xml',(35,0)),('scientific.xml',(700,13)),('preflight.xml',(1087,0))]:
    cases=ET.parse(out/filename).findall('.//testcase');assert all(all(c.find(t) is None for t in ('failure','error')) for c in cases)
    skipped=sum(c.find('skipped') is not None for c in cases);pair=(len(cases)-skipped,skipped);assert pair==expected,(filename,pair)
    counts[filename]=dict(passed=pair[0],skipped=pair[1])
science=module;science.python(ET.parse(out/'scientific.xml').findall('.//testcase'),gui=False)
matrices=[]
fields=['case_name','source_bounds','nominal_downsample','core_shape','context','independent_processing_rgb_exact','feature_checks','level_zero_segmentation','processing_segmentation','foreground_pixel_differences','processing_label_sha256','level_zero_label_sha256']
for name in ('roi-context0','roi-context32','full-context0','physical-context32'):
    a=json.loads((out/'windows'/(name+'.json')).read_text(encoding='utf-8'));b=json.loads((out/'linux'/(name+'.json')).read_text(encoding='utf-8'))
    assert a['build_git_revision']==manifest['source'] and b['build_git_revision']==linux['native_source']
    assert a['comparison_tool_sha256']==b['comparison_tool_sha256']==digest(work/'tools/compare_qupath_nominal_cells.py')
    assert a['frozen_comparison_tolerances']==b['frozen_comparison_tolerances'] and len(a['cases'])==len(b['cases'])
    for x,y in zip(a['cases'],b['cases']):
        for field in fields:assert x[field]==y[field],(name,x['case_name'],field)
        left=[(f['compartment'],f['reference_label'],f['native_label'],f['same_sampling_domain'],f['within_frozen_tolerance']) for f in x['matched_reference_feature_differences']]
        right=[(f['compartment'],f['reference_label'],f['native_label'],f['same_sampling_domain'],f['within_frozen_tolerance']) for f in y['matched_reference_feature_differences']]
        assert left==right
    matrices.append(dict(name=name,cases=len(a['cases']),exact_cross_host_fields=fields,feature_domain_decisions_equal=True))
summary=dict(schema='qpx.nominal-cells-corpus-local/1',observer_source=observer,windows_native_source=manifest['source'],linux_native_source=linux['native_source'],
    windows_native_sha256=digest(Path(native.__file__)),linux_wheel_sha256=linux['wheel_sha256'],source_paths=len(merged),unchanged_compiler_inputs=424,
    unchanged_windows_products=71,unchanged_windows_package_members=len(installed['members']),linux_package_members=len(linux['installed']),native_rebuilt=False,
    tests=counts,counts_overlap=True,matrices=matrices,exact_level_zero_mask_cases=8,total_pixel_cases=9,
    roi_difference_retained=True,processing_and_level_zero_feature_domains_separate=True,shared_test_dependencies=True,clean_machine=False,
    own_source_platform_qualification=False,segmentation_parity_accepted=False,biological_validation=False,general_speed_claim=False,release_ready=False)
(out/'source-runtime-proof.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
print('Two-host nominal corpus:35 new checks each,700/13 scientific,1087 controls,896 source paths; no native rebuild',flush=True)
