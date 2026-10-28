from pathlib import Path
import json
root=Path(__file__).resolve().parents[2]
out=root/'build/gui-font-qualification-285'
state=json.loads((out/'campaign.json').read_text())
assert set(state['roles'])=={'engine','macos','portable'}
assert all(v['state']=='run_identified' for v in state['roles'].values())
collector=(root/'build/ci/collect_candidate281.py').read_text()
assert 'stream-header-qualification-281' in collector and 'q281-all' in collector
collector=collector.replace('candidate281','font candidate285').replace('stream-header-qualification-281','gui-font-qualification-285').replace('q281-all','q285-all')
target=root/'build/ci/collect_candidate285.py'
assert not target.exists()
target.write_text(collector,encoding='utf-8')
plan=json.loads((root/'build/stream-header-qualification-281/complete-audit-plan.json').read_text())
old_runs={'engine':'40726273936','macos':'40726289400','portable':'40726306463'}
for item in plan:
    role=item['role']
    replacements={
        'stream-header-qualification-281':'gui-font-qualification-285',
        'q281-all':'q285-all',
        '3f15c2be8722ec6559d8657ecd69a839e1f200a5':state['source'],
        old_runs[role]:str(state['roles'][role]['run']['databaseId'])}
    for key in ('argv','cwd'):
        values=item[key] if isinstance(item[key],list) else [item[key]]
        for old,new in replacements.items():
            values=[v.replace(old,new) for v in values]
        item[key]=values if isinstance(item[key],list) else values[0]
    item['state']='prepared_not_executed'
    item['requires']='terminal role and all applicable authenticated archives; frozen285 original auditors only'
(out/'complete-audit-plan.json').write_text(json.dumps(plan,indent=2)+'\n')
print('Prepared immutable-source285 complete audit plan and observer-only collector')
