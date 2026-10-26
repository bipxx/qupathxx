from pathlib import Path
import hashlib,importlib.util,json,shutil,subprocess,xml.etree.ElementTree as E
root=Path('.').resolve();out=root.parents[1]/'raw-contract-250';dest=root/'docs/benchmarks/raw-classifier-contract-250';dest.mkdir(exist_ok=True)
p=root/'tools/ci/raw_classifier_evidence.py';spec=importlib.util.spec_from_file_location('raw_contract_250',p);m=importlib.util.module_from_spec(spec);exec(compile(p.read_bytes(),str(p),'exec'),m.__dict__)
commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip();proof=m.source_contract(root,commit);assert m.common.verify_source_snapshot(root,proof)==len(proof)
(out/'source-contract.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8')
report={'schema':'qpx.raw-classifier-contract-local/1','contract_source':commit,'runtime_source':'3039f0c23b274a8b0f121e48df0d290447ecf2e3','source_paths':len(proof),'scope':'Local producer/auditor domain admission using unchanged history248 installed package and retained Windows/Clang native outputs; not integrated qualification or Mac repair.'}
for mode in ['headless','gui']:
 report[mode]=m.python(list(E.parse(out/(mode+'.xml')).getroot().iter('testcase')),gui=mode=='gui')
report['typing']=m.typing(out)
for label,where,full in [('windows','final',True),('clang_minimal','clang-minimal',False)]:
 source=root.parents[1]/'raw-stream-history-248'/where/'native-tests.xml';shutil.copyfile(source,out/(label+'-native-tests.xml'))
 report[label]=m.native(list(E.parse(source).getroot().iter('testcase')),full=full)
installed=json.loads((root.parents[1]/'raw-stream-history-248/final/installed-proof.json').read_text(encoding='utf-8'))
wheel=Path(installed['wheel']);assert hashlib.sha256(wheel.read_bytes()).hexdigest()==installed['wheel_sha256']
for member in installed['wheel_members']:
 f=Path(installed['package']).parent/member['path'];data=f.read_bytes()
 assert len(data)==member['bytes'] and hashlib.sha256(data).hexdigest()==member['sha256'],str(f)
report['installed_members_verified']=len(installed['wheel_members']);report['wheel_sha256']=installed['wheel_sha256'];report['native_rebuild']=False
shutil.copyfile(root.parents[1]/'raw-stream-history-248/final/installed-proof.json',out/'unchanged-installed-proof.json')
controls=list(E.parse(out/'controls.xml').getroot().iter('testcase'))
assert len(controls)==183 and all(c.find(tag) is None for c in controls for tag in ['failure','error','skipped'])
report['controls']={'passed':len(controls),'initial_controls_passed':169,'real_minimal_admission_initially_rejected':True,'rejection_preserved':'initial-native-admission-failure.json'}
report['limitations']=['No hosted qualification dispatched','Mac observer run40411025705 queued at last observation; root cause/repair unproven','Native outputs retain original3039f0c source and flags','Windows installed tests share existing test dependencies; not a clean-machine release','No scientific, process-memory/hardware or release acceptance']
for file in sorted(out.iterdir()):
 if file.is_file():shutil.copyfile(file,dest/file.name)
report['evidence']=[{'path':f.name,'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()} for f in sorted(dest.iterdir()) if f.is_file() and f.name!='receipt.json']
(dest/'receipt.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({k:report[k] for k in ['contract_source','source_paths','installed_members_verified','controls']},indent=2))
print('Real headless and GUI cohorts, typing and retained native semantics admitted.')
