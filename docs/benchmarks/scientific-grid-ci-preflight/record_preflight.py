from pathlib import Path
import datetime,hashlib,importlib.util,json,shutil,subprocess,xml.etree.ElementTree as E
root=Path(r'C:/Users/HEQ/Projects/qupathxx');source=root/'build/ci/scientific-grid-159';frozen=root/'build/ci/scientific-workflow-168';commit='5ebdb961e7c9c2bad8c82367ad090eb23ba4ec57'
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=frozen,text=True).strip()==commit
assert not subprocess.check_output(['git','status','--porcelain'],cwd=frozen,text=True)
spec=importlib.util.spec_from_file_location('science',frozen/'tools/ci/scientific_grid_evidence.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
proof=m.source_contract(frozen,commit)
out=source/'docs/benchmarks/scientific-grid-ci-preflight';out.mkdir(exist_ok=True)
(out/'frozen-source-proof.json').write_bytes((json.dumps(dict(source=commit,files=proof),indent=2)+'\n').encode())
for name in ('gridci168-expanded-auditor-tests.xml','gridci168-macos-auditor-tests.xml','gridci168-installed-headless-tests.xml','gridci168-campaign.json'):shutil.copyfile(root/'build'/name,out/name)
for p in (root/'build/gridci168-checks').glob('*'):shutil.copyfile(p,out/p.name)
for p in (root/'build/gridci168-hosted').glob('*.json'):shutil.copyfile(p,out/p.name)
for name in ('dispatch_gridci168.py','dispatch_gridci168_corrected.py','poll_gridci168.py'):shutil.copyfile(root/'build/ci'/name,out/name)
shutil.copyfile(__file__,out/'record_preflight.py')
def tests(name,count,skips=0):
    values=list(E.parse(out/name).getroot().iter('testcase'));assert len(values)==count+skips and all(c.find(t) is None for c in values for t in ('failure','error'))
    assert sum(c.find('skipped') is not None for c in values)==skips
for name,count,skips in [('gridci168-expanded-auditor-tests.xml',185,0),('gridci168-macos-auditor-tests.xml',14,0),('gridci168-installed-headless-tests.xml',104,2)]:tests(name,count,skips)
campaign=json.loads((out/'gridci168-campaign.json').read_text(encoding='utf-8'));attempts=[a for a in campaign['attempts'] if a.get('run')]
assert {a['role'] for a in attempts}=={'engine','macos','portable'} and all(a['run']['headSha']==commit for a in attempts)
receipt=dict(schema='qpx.scientific-grid-ci-preflight/1',source=commit,observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),frozen_ref=campaign['ref'],source_files_verified=len(proof),affected_auditor_tests_passed=185,macos_relocation_auditor_tests_passed=14,installed_headless_passed=104,declared_headless_desktop_skips=2,retained_evidence=json.loads((out/'retained-and-typing.json').read_text(encoding='utf-8')),campaign=campaign,
    scope='CI harness integration and local auditor development checks. The installed/native evidence is explicitly reused from frontend167 because all 298 compiler inputs are unchanged. Hosted builders/consumers must still compile and qualify the exact frozen candidate. No hosted acceptance is inferred from source comparison or successful dispatch.',
    compiler_input_reuse=json.loads((out/'preflight.json').read_text(encoding='utf-8')),platform_campaign_accepted=False,scientific_detector_accepted=False,release_ready=False)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
receipt['evidence']={p.name:dict(bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(out.iterdir()) if p.is_file()}
(source/'docs/benchmarks/scientific-grid-ci-preflight-windows.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode())
print(json.dumps(dict(source=commit,verified_source_files=len(proof),runs={a['role']:a['run']['databaseId'] for a in attempts},platform_campaign_accepted=False),indent=2))
