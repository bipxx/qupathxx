from pathlib import Path
from datetime import datetime,timezone
import hashlib,importlib.util,json,shutil,subprocess,xml.etree.ElementTree as ET
import qupathxx as q,qupathxx._native as native
root=Path(__file__).resolve().parent/'scientific-grid-159';base=root.parents[1]
win=base/'grid172-local';linux=base/'grid172-linux-local';repeat=base/'grid172-resampled-capture-repeat'
folder=root/'docs/benchmarks/grid-cell-resampled-processing-local';folder.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True,encoding='utf-8').strip()
spec=importlib.util.spec_from_file_location('sampling_audit',root/'tools/ci/scientific_grid_evidence.py');audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
proof=json.loads((win/'source-proof.json').read_text(encoding='utf-8'));assert proof['source']==source and audit.source_contract(root,source)==proof['files']
old=json.loads((root/'docs/benchmarks/scientific-grid-portability-fix-local/compiler-input-proof.json').read_text(encoding='utf-8'))
assert all(sha(root/f['path'])==f['checkout_sha256'] for f in old['files'])
assert sha(Path(native.__file__))=='bc286d4d160c59fbccf853173820ed88280ddc4ef28e0ce31cc3553e9c9755d0'
linux_identity=json.loads((linux/'identity.log').read_text(encoding='utf-8'))
qualified=json.loads((base/'gridci169-hosted/full-engine-audit.json').read_text(encoding='utf-8'));job=next(j for j in qualified['jobs'] if 'Linux headless' in j['name'])
assert linux_identity['sha256']==job['linux_runtime']['native_sha256'] and linux_identity['revision']=='3fc3bcb460ce590d9f1a0c48d97ca8a6dc400062'
wheel=base/'gridci169-hosted/artifacts/Linux-headless-py3.12/wheels'/job['artifact']['wheel'];assert sha(wheel)==job['artifact']['wheel_sha256']
cohorts={};reports={}
for side,path in [('windows',win),('linux',linux)]:
 cases=list(ET.parse(path/'tests.xml').getroot().iter('testcase'))
 assert len(cases)==307 and not any(c.find(k)is not None for c in cases for k in ['failure','error']) and sum(c.find('skipped')is None for c in cases)==305
 cohorts[side]=audit.python(cases,False);assert cohorts[side]['passed']==177
 for module,count in [('test_qupath_grid_cells_reference',32),('test_qupath_roi_cell_debug',14),('test_qupath_resampled_cell_debug',26)]:
  selected=[c for c in cases if c.get('classname','').rsplit('.',1)[-1]==module];assert len(selected)==count and all(c.find('skipped')is None for c in selected)
 report=json.loads((path/'resampled-native.json').read_text(encoding='utf-8'));reports[side]=report
 assert report['processing_dimensions']==[171,128] and report['nominal_step']==[3,2]
 assert report['nominal_bilinear_half_up_different_components']==0 and report['realized_width_bilinear_different_components']==654 and report['ties_even_bilinear_different_components']==32
 assert report['original_source_area_different_components']==951 and report['original_source_area_max_absolute_rgb_difference']==48.1328125
 assert not any(report[k] for k in ['original_source_replay_authority','segmentation_parity_accepted','sampler_equivalence_accepted','biological_validation','model_generalized'])
 for metric in report['segmentation'].values():
  assert metric['reference_count']==metric['native_count']==3 and metric['foreground_iou']==1
  assert all(m['iou']==1 and m['centroid_error_px']==0 for m in metric['matches'])
 assert all(f[k] for f in report['feature_checks'] for k in ['sample_count_exact','physical_area_exact','rgb_accepted','od_accepted'])
assert reports['windows']['segmentation']==reports['linux']['segmentation'] and reports['windows']['feature_checks']==reports['linux']['feature_checks']
for name in ['tests.xml','installed.log','pixels.json','pixels.log','micrometres.json','micrometres.log','roi-cropped.json','roi-cropped.log','debug-verification.json','debug-verification.log','resampled-native.json','resampled-native.log','source-proof.json']:
 shutil.copyfile(win/name,folder/name)
for name in ['tests.xml','installed.log','identity.log','roi-cropped.json','roi-cropped.log','resampled-native.json','resampled-native.log']:
 shutil.copyfile(linux/name,folder/('linux-'+name))
shutil.copyfile(base/'grid172-repeat-identities.json',folder/'repeat-identities.json')
for name in ['driver.log','manifest.json','debug-stack.tif']:
 shutil.copyfile(repeat/name,folder/('repeat-'+name))
for name in ['verify_grid172.py','verify_grid172_linux.py','probe_grid172_mapping.py','probe_grid172_pixels.py','probe_grid172_bilinear.py','probe_grid172_geometry.py']:
 shutil.copyfile(Path(__file__).with_name(name),folder/name)
shutil.copyfile(Path(__file__),folder/'record_evidence.py')
repeated=json.loads((folder/'repeat-identities.json').read_text(encoding='utf-8'));assert repeated['non_log_identical']==14 and all(v['equal'] for k,v in repeated['payloads'].items() if k!='driver.log')
record=dict(schema='qpx.grid-cell-resampled-processing-local/1',observed_at_utc=datetime.now(timezone.utc).isoformat(),harness_source=source,
 application_source='3fc3bcb460ce590d9f1a0c48d97ca8a6dc400062',compiler_inputs_unchanged=len(old['files']),source_contract_files=len(proof['files']),
 windows_native=dict(sha256=sha(Path(native.__file__)),compiled_revision=q.build_environment().git_revision,source_tree_sha256=q.build_environment().source_tree_sha256),
 linux_native=linux_identity,linux_wheel_sha256=sha(wheel),linux_wheel_qualified_run=40188586241,
 installed_passed_per_platform=305,declared_desktop_skips_per_platform=2,named_grid_cohorts=cohorts,
 resampled_capture_checks_passed_per_platform=26,repeat_non_log_payloads_identical=14,
 processing_dimensions=[171,128],nominal_step=[3,2],realized_x_step=[256,171],captured_processing_detector_masks_and_features_exact=True,
 original_source_area_input_different_components=951,original_source_area_input_max_absolute_difference=48.1328125,
 pixel_model_scope='Nominal center-bilinear half-up matches this one captured synthetic input; no generalized Java/native compatibility claim',
 local_windows_linux_comparison_accepted=True,native_production_and_original_fixtures_unchanged=True,legacy_distance_expected_failures_unchanged=True,
 original_source_replay_authority=False,sampler_equivalence_accepted=False,segmentation_parity_accepted=False,platform_campaign_accepted=False,biological_validation=False,release_ready=False,overall_project_complete=False,
 next='Typed raw scientific sampling and shared analysis workflow, preserving all sample types, source/axis/finite semantics and original M0-M7 scope',
 evidence={p.name:dict(bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(folder.iterdir()) if p.is_file()})
(root/'docs/benchmarks/grid-cell-resampled-processing-local-windows-linux.json').write_bytes((json.dumps(record,indent=2)+'\n').encode('utf-8'))
print(json.dumps(dict(source=source,passed_per_platform=305,resampled_checks=26,source_files=len(proof['files']),receipt=str(root/'docs/benchmarks/grid-cell-resampled-processing-local-windows-linux.json'))))
