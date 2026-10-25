from pathlib import Path
import datetime,hashlib,importlib.util,json,subprocess,xml.etree.ElementTree as ET
import qupathxx as q
import qupathxx._native as native
root=Path.cwd(); source=root/'build/ci/scientific-grid-159'; out=root/'build/raw187-layouts';out.mkdir(exist_ok=True)
commit=subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()
assert not subprocess.check_output(['git','-C',str(source),'status','--porcelain'],text=True).strip()
old=json.loads((source/'docs/benchmarks/raw-grid-workspace-local-windows-linux.json').read_text(encoding='utf-8'))
baseline=json.loads((source/'docs/benchmarks/raw-grid-workspace-local/compiled-source-proof.json').read_text(encoding='utf-8'))
inputs=sorted(set([*source.glob('cpp/**/*.cpp'),*source.glob('cpp/**/*.hpp'),*source.glob('cmake/**/*.cmake'),*source.glob('cmake/**/*.in'),*source.glob('src/**/*.py'),source/'CMakeLists.txt',source/'pyproject.toml']))
manifest={p.relative_to(source).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
assert len(manifest)==311
for name,digest in manifest.items(): assert digest==baseline['files'][name]['checked_out_sha256'],name
fingerprint=hashlib.sha256(''.join(name+'='+digest+'\n' for name,digest in sorted(manifest.items())).encode()).hexdigest()
environment=q.build_environment()
assert environment.source_tree_sha256==fingerprint==old['source_tree_sha256']
assert environment.git_revision.startswith(old['compiled_source'])
package=Path(q.__file__).parent; inventory={}
for name,expected in old['candidate_inventory'].items():
 p=package/name;actual=dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest());assert actual==expected,name;inventory[name]=actual
spec=importlib.util.spec_from_file_location('qpx_raw187_scientific_evidence',source/'tools/ci/scientific_grid_evidence.py');e=importlib.util.module_from_spec(spec);spec.loader.exec_module(e)
proof=e.source_contract(source,commit)
(out/'source-proof.json').write_bytes((json.dumps(dict(source=commit,files=proof),indent=2)+'\n').encode())
(out/'compiler-inputs.json').write_bytes((json.dumps(dict(compiled_source=old['compiled_source'],delivery_source=commit,fingerprint=fingerprint,files=manifest),indent=2)+'\n').encode())
cases=list(ET.parse(root/'build/raw187-scientific-cohort.xml').getroot().iter('testcase'))
cohort=e.python(cases,False);assert cohort['passed']==243 and cohort['declared_headless_gui_skips']==5
reader=list(ET.parse(root/'build/raw187-reader-auditor.xml').getroot().iter('testcase'));assert len(reader)==216 and not any(c.find(k) is not None for c in reader for k in ('failure','error','skipped'))
report=dict(schema='qpx.quantitative-tiff-layout-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),compiled_source=old['compiled_source'],delivery_source=commit,compiler_input_files=311,source_tree_sha256=fingerprint,source_contract_files=len(proof),module_path=str(Path(native.__file__).resolve()),python_package=str(package.resolve()),candidate_inventory=inventory,scientific_cohort=cohort,new_layout_cases=30,reader_and_auditor_passed=216,first_attempt='27 fixture setup failures: ImageServer has no context manager; use contextlib.closing. No native change.',scope='Local installed Windows synthetic typed TIFF reader/sampler integration; not a new platform campaign, biological or supported-release acceptance.',platform_campaign_accepted=False,release_ready=False,overall_project_complete=False)
(out/'report.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
print(json.dumps({k:report[k] for k in ['compiled_source','delivery_source','compiler_input_files','source_contract_files','new_layout_cases','reader_and_auditor_passed','scientific_cohort']},indent=2))
