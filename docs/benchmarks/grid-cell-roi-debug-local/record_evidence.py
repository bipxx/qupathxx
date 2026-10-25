from pathlib import Path
from datetime import datetime,timezone
import hashlib,importlib.util,json,shutil,subprocess,sys,xml.etree.ElementTree as ET
import qupathxx as q,qupathxx._native as native
root=Path(__file__).resolve().parent/'scientific-grid-159'
base=root.parents[1];win=base/'grid171-local';linux=base/'grid171-linux-local';raw=base/'grid171-roi-debug'
folder=root/'docs/benchmarks/grid-cell-roi-debug-local';folder.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True,encoding='utf-8').strip()
spec=importlib.util.spec_from_file_location('roi_grid_audit',root/'tools/ci/scientific_grid_evidence.py');audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
proof=json.loads((win/'source-proof.json').read_text(encoding='utf-8'));assert proof['source']==source and audit.source_contract(root,source)==proof['files']
previous=json.loads((root/'docs/benchmarks/scientific-grid-portability-fix-local/compiler-input-proof.json').read_text(encoding='utf-8'))
assert all(sha(root/f['path'])==f['checkout_sha256'] for f in previous['files'])
assert sha(Path(native.__file__))=='bc286d4d160c59fbccf853173820ed88280ddc4ef28e0ce31cc3553e9c9755d0'
identity=json.loads((linux/'identity.log').read_text(encoding='utf-8'))
qualified=json.loads((base/'gridci169-hosted/full-engine-audit.json').read_text(encoding='utf-8'))
job=next(j for j in qualified['jobs'] if 'Linux headless' in j['name'])
wheel=base/'gridci169-hosted/artifacts/Linux-headless-py3.12/wheels'/job['artifact']['wheel']
assert sha(wheel)==job['artifact']['wheel_sha256'] and identity['sha256']==job['linux_runtime']['native_sha256']
assert identity['revision']=='3fc3bcb460ce590d9f1a0c48d97ca8a6dc400062'
cohorts={}
for side,path in [('windows',win),('linux',linux)]:
 cases=list(ET.parse(path/'tests.xml').getroot().iter('testcase'))
 assert len(cases)==277 and not any(c.find(k)is not None for c in cases for k in ['failure','error'])
 assert sum(c.find('skipped')is None for c in cases)==275
 cohorts[side]=audit.python(cases,False);assert cohorts[side]['passed']==151
 for module,count in [('test_qupath_grid_cells_reference',32),('test_qupath_roi_cell_debug',14)]:
  selected=[c for c in cases if c.get('classname','').rsplit('.',1)[-1]==module]
  assert len(selected)==count and all(c.find('skipped')is None for c in selected)
 report=json.loads((path/'roi-cropped.json').read_text(encoding='utf-8'))
 roi=next(c for c in report['cases'] if c['case_name']=='roi_cut_hole')
 assert roi['segmentation']['nucleus']['foreground_iou']==768/772 and roi['segmentation']['body']['foreground_iou']==1
for name in ['tests.xml','installed.log','pixels.json','pixels.log','micrometres.json','micrometres.log','roi-cropped.json','roi-cropped.log','debug-verification.json','debug-verification.log','source-proof.json','native-kernel-probe.json','native-kernel-compiler.log','domain-comparison.json','repeat-capture.json','repeat-manifest.json','repeat-driver.log','repeat-debug-stack.tif']:
 shutil.copyfile(win/name,folder/name)
for name in ['tests.xml','installed.log','identity.log','environment.json','install.log','venv.log','roi-cropped.json','roi-cropped.log']:
 shutil.copyfile(linux/name,folder/('linux-'+name))
for name in ['native-kernel-probe.cpp','native-input-od-f64le.bin']:
 shutil.copyfile(raw/name,folder/name)
for name in ['verify_grid171.py','verify_grid171_linux.py','probe_grid171_domains.py','export_grid171_native_score.py']:
 shutil.copyfile(Path(__file__).with_name(name),folder/name)
shutil.copyfile(Path(__file__),folder/'record_evidence.py')
repeat=json.loads((win/'repeat-capture.json').read_text(encoding='utf-8'));assert repeat['non_log_payloads_identical']==12
domains=json.loads((win/'domain-comparison.json').read_text(encoding='utf-8'))
assert len(domains)==4
for experiment in domains:
 nucleus=experiment['compartments']['nucleus'];body=experiment['compartments']['body']
 assert body['iou']==1 and not body['native_only'] and not body['reference_only']
 assert (nucleus['native_only'],nucleus['reference_only'])==((0,4) if experiment['context']==0 else (24,0))
debug=json.loads((win/'debug-verification.json').read_text(encoding='utf-8'))
assert debug['ordinary_labels_exact'] and debug['debug_nucleus_masks_exact'] and debug['processing_bounds']==[32,32,192,128]
record=dict(schema='qpx.grid-cell-roi-debug-local/1',observed_at_utc=datetime.now(timezone.utc).isoformat(),harness_source=source,
 application_source='3fc3bcb460ce590d9f1a0c48d97ca8a6dc400062',source_contract_files=len(proof['files']),compiler_inputs_unchanged=len(previous['files']),
 windows_native=dict(sha256=sha(Path(native.__file__)),compiled_revision=q.build_environment().git_revision,source_tree_sha256=q.build_environment().source_tree_sha256),
 linux_native=identity,linux_wheel_sha256=sha(wheel),linux_wheel_qualified_run=40188586241,
 installed_passed_per_platform=275,declared_desktop_skips_per_platform=2,named_grid_cohorts=cohorts,
 grid_comparison_passed_per_platform=32,debug_capture_checks_passed_per_platform=14,
 repeat_capture_non_log_payloads_identical=12,ordinary_capture_masks_and_values_exact=True,
 processing_bounds=[32,32,192,128],full_image_native_only_nucleus_pixels=24,matched_crop_reference_only_nucleus_pixels=4,
 cell_body_masks_exact=True,kernel_probe_scope='Standalone GCC 13.3 WSL C++ kernel with installed Windows float64 OD input; no production native rebuild or complete new platform campaign',
 remaining=['Requested-resampling source/grid mapping','Distinct boundary and arithmetic recipes','Biological validation and wider M0-M7 scope'],
 native_production_and_original_fixtures_unchanged=True,legacy_distance_expected_failures_unchanged=True,
 local_windows_linux_comparison_accepted=True,platform_campaign_accepted=False,scientific_acceptance=False,biological_validation=False,release_ready=False,overall_project_complete=False,
 evidence={p.name:dict(bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(folder.iterdir()) if p.is_file()})
(root/'docs/benchmarks/grid-cell-roi-debug-local-windows-linux.json').write_bytes((json.dumps(record,indent=2)+'\n').encode('utf-8'))
print(json.dumps(dict(source=source,passed_per_platform=275,new_comparison=32,debug_capture=14,source_files=record['source_contract_files'],receipt=str(root/'docs/benchmarks/grid-cell-roi-debug-local-windows-linux.json'))))
