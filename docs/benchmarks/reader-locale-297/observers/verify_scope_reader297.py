from pathlib import Path
import hashlib
import json
import re
import subprocess
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159'
path='docs/project-plan.md'
old=subprocess.check_output(['git','show','a3868f57410c39bb80a9cf9455f413ed147f11a6:'+path],cwd=work).decode()
new=(work/path).read_text()
headings=lambda text:re.findall(r'^#{1,3} .+$',text,re.M)
assert headings(old)==headings(new) and len(headings(new))==19
start='## Milestones and acceptance criteria';end='## Execution order from the present state'
scope=lambda text:text[text.index(start):text.index(end)]
assert scope(old)==scope(new)
start='## Current execution map';end='## Execution order from the present state'
assert old[old.index(start):old.index(end)]==new[new.index(start):new.index(end)]
target=work/'docs/benchmarks/reader-locale-297/plan-scope-proof.json'
target.write_text(json.dumps(dict(previous_source='a3868f57410c39bb80a9cf9455f413ed147f11a6',headings_preserved=19,all_milestone_acceptance_and_scope_preserved=True,current_execution_map_through_scope_preserved=True,scope_sha256=hashlib.sha256(scope(new).encode()).hexdigest()),indent=2)+'\n')
receipt_path=target.with_name('receipt.json');receipt=json.loads(receipt_path.read_text())
receipt['files'].append(dict(path=target.name,bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
source=Path(__file__);dest=target.parent/'observers'/source.name;dest.write_bytes(source.read_bytes())
receipt['files'].append(dict(path='observers/'+source.name,bytes=dest.stat().st_size,sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))
receipt_path.write_text(json.dumps(receipt,indent=2)+'\n')
print('All19 plan headings and full milestone/scope block preserved')
