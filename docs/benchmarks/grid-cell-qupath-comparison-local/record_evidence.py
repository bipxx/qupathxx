from pathlib import Path
from datetime import datetime,timezone
import hashlib,importlib.util,json,shutil,subprocess,sys,xml.etree.ElementTree as ET
import numpy,scipy,PIL,qupathxx as q,qupathxx._native as native
root=Path(__file__).resolve().parent/'scientific-grid-159'
raw=root.parents[1]/'grid170-local'
dest=root/'docs/benchmarks/grid-cell-qupath-comparison-local'
dest.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True,encoding='utf-8').strip()
proof=json.loads((raw/'source-proof.json').read_text(encoding='utf-8'));assert proof['source']==source
spec=importlib.util.spec_from_file_location('grid_evidence',root/'tools/ci/scientific_grid_evidence.py');audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
assert audit.source_contract(root,source)==proof['files']
previous=json.loads((root/'docs/benchmarks/scientific-grid-portability-fix-local/compiler-input-proof.json').read_text(encoding='utf-8'))
assert all(sha(root/m['path'])==m['checkout_sha256'] for m in previous['files'])
installed=Path(native.__file__);assert sha(installed)=='bc286d4d160c59fbccf853173820ed88280ddc4ef28e0ce31cc3553e9c9755d0'
cases=list(ET.parse(raw/'tests.xml').getroot().iter('testcase'))
assert len(cases)==254 and not any(c.find(k)is not None for c in cases for k in ['failure','error'])
cohort=audit.python(cases,False);assert cohort['passed']==132
comparison=[c for c in cases if c.get('classname','').rsplit('.',1)[-1]=='test_qupath_grid_cells_reference']
assert len(comparison)==27 and all(c.find('skipped')is None for c in comparison)
reports={u:json.loads((raw/(u+'.json')).read_text(encoding='utf-8')) for u in ['pixels','micrometres']}
assert len(reports['pixels']['cases'])==9 and len(reports['micrometres']['cases'])==8
assert all(d['native_sha256']==sha(installed) and not d['segmentation_parity_accepted'] and not d['biological_validation'] for d in reports.values())
for name in ['tests.xml','installed.log','pixels.json','pixels.log','micrometres.json','micrometres.log','source-proof.json','roi-diagnostic.json']:
 shutil.copyfile(raw/name,dest/name)
shutil.copyfile(Path(__file__),dest/'record_evidence.py')
shutil.copyfile(Path(__file__).with_name('verify_grid170.py'),dest/'verify_installed.py')
shutil.copyfile(Path(__file__).with_name('diagnose_grid170_roi.py'),dest/'diagnose_roi.py')
record=dict(schema='qpx.grid-cell-qupath-comparison-local/1',observed_at_utc=datetime.now(timezone.utc).isoformat(),
 harness_source=source,application_source='3fc3bcb460ce590d9f1a0c48d97ca8a6dc400062',
 native=dict(path=str(installed),sha256=sha(installed),compiled_revision=q.build_environment().git_revision,source_tree_sha256=q.build_environment().source_tree_sha256),
 compiler_inputs_unchanged=len(previous['files']),source_contract_files=len(proof['files']),
 environment=dict(python=sys.version,numpy=numpy.__version__,scipy=scipy.__version__,pillow=PIL.__version__),
 installed_passed=252,declared_desktop_skips=2,named_grid_cohort=cohort,new_comparison_passed=27,
 scope='Pinned synthetic explicit-unit-grid RGB cell masks/features, independent source arithmetic, context 0/32 and equivalent isotropic physical recipes; not general scientific acceptance',
 exact_natural_grid_cases=['blank','isolated_three','touching_pair','gap_pair','size_filters','gradient_noise','anisotropic_level0'],
 remaining_differences=['Clipped ROI nucleus mask','Requested-resampling grid is not equivalent','Anisotropic physical metric differs from averaged-axis QuPath conversion'],
 legacy_distance_expected_failures_unchanged=True,platform_campaign_accepted=False,biological_validation=False,release_ready=False,overall_project_complete=False,
 evidence={p.name:dict(bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(dest.iterdir()) if p.is_file()})
(root/'docs/benchmarks/grid-cell-qupath-comparison-local-windows.json').write_bytes((json.dumps(record,indent=2)+'\n').encode('utf-8'))
print(json.dumps(dict(source=source,installed=record['installed_passed'],new=record['new_comparison_passed'],compiler_reused=record['compiler_inputs_unchanged'],receipt=str(root/'docs/benchmarks/grid-cell-qupath-comparison-local-windows.json'))))
