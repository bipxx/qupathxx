from pathlib import Path
import datetime,hashlib,importlib.util,json,subprocess,xml.etree.ElementTree as ET
root=Path.cwd();source=root/'build/ci/raw-grid-workflow-191';out=root/'build/raw191-preflight';out.mkdir(exist_ok=False)
commit='61e546ee59944e07b2b15a16b84054813de8393e'
assert subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()==commit
assert not subprocess.check_output(['git','-C',str(source),'status','--porcelain']).strip()
previous=json.loads((root/'build/raw186-preflight-after/report.json').read_text());files=[source/'tests'/name for name in previous['test_sources']]
command=[str(root/'build/ci/raw190-python-env/Scripts/python.exe'),'-I','-m','pytest',*map(str,files),'-q','--tb=short','--junitxml='+str(out/'tests.xml')]
with (out/'tests.log').open('wb') as log:p=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=300)
assert p.returncode==0,(out/'tests.log').read_text(encoding='utf-8',errors='replace')
cases=list(ET.parse(out/'tests.xml').getroot().iter('testcase'));assert len(cases)==280 and not any(c.find(k) is not None for c in cases for k in ['failure','error','skipped'])
spec=importlib.util.spec_from_file_location('raw191_frozen_sources',source/'tools/ci/scientific_grid_evidence.py');e=importlib.util.module_from_spec(spec);spec.loader.exec_module(e);proof=e.source_contract(source,commit);assert len(proof)==498
(out/'source-proof.json').write_bytes((json.dumps(dict(source=commit,files=proof),indent=2)+'\n').encode())
inputs=sorted(set([*source.glob('cpp/**/*.cpp'),*source.glob('cpp/**/*.hpp'),*source.glob('cmake/**/*.cmake'),*source.glob('cmake/**/*.in'),*source.glob('src/**/*.py'),source/'CMakeLists.txt',source/'pyproject.toml']))
manifest={p.relative_to(source).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs};assert len(manifest)==312
raw_hash=hashlib.sha256(''.join(n+'='+v+'\n' for n,v in sorted(manifest.items())).encode()).hexdigest()
subprocess.run(['git','-C',str(source),'diff','--quiet','44394d3db15b37659e06b10677498ec168041e4d',commit,'--','cpp','cmake','src','CMakeLists.txt','pyproject.toml'],check=True)
(out/'frozen-compiler-inputs.json').write_bytes((json.dumps(dict(source=commit,raw_checkout_fingerprint=raw_hash,files=manifest),indent=2)+'\n').encode())
report=dict(source=commit,observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=280,scientific_source_contract_files=498,compiler_inputs=312,command=command,test_sources={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in files},local_module_compiled_source='44394d3db15b37659e06b10677498ec168041e4d',local_module_fingerprint='7eb7afb7850fa3d35caeefd842dfb2dcb6bfdee5cd9da050f1d3deff80cd8d53',frozen_checkout_raw_fingerprint=raw_hash,scope='Frozen-source auditor preflight and byte proof only; not platform campaign acceptance. Compiler Git blobs match tested443; checkout line endings have distinct raw identities.',release_ready=False)
(out/'report.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps({k:report[k] for k in ['source','passed','scientific_source_contract_files','compiler_inputs','frozen_checkout_raw_fingerprint']},indent=2))
