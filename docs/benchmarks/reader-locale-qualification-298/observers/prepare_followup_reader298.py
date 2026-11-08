from pathlib import Path
import json
root=Path(__file__).resolve().parents[2];out=root/'build/reader-locale-qualification-298'
state=json.loads((out/'campaign.json').read_text())
assert set(state['roles'])=={'engine','macos','portable'}
assert all(v['state']=='run_identified' for v in state['roles'].values())
text=(root/'build/ci/collect_candidate295.py').read_text().replace('nominal-cell-qualification-295','reader-locale-qualification-298').replace('candidate295','candidate298').replace('q295-all','q298-all')
target=root/'build/ci/collect_candidate298.py';assert not target.exists();target.write_text(text,encoding='utf-8')
plan=json.loads((root/'build/nominal-cell-qualification-295/complete-audit-plan.json').read_text())
old={'engine':'40782637825','macos':'40782648204','portable':'40782658569'}
for item in plan:
    role=item['role'];item['cwd']=str(root/'build/ci/reader-locale-qualification-298-audit')
    item['argv']=[v.replace('nominal-cell-qualification-295','reader-locale-qualification-298').replace('q295-all','q298-all').replace('a7eb78de75387bce2da7b3d59b7f0a2ab6126cee',state['source']).replace(old[role],str(state['roles'][role]['run']['databaseId'])) for v in item['argv']]
    item['requires']='Terminal role plus every applicable authenticated archive; frozen298 original complete auditors only'
target=out/'complete-audit-plan.json';assert not target.exists();target.write_text(json.dumps(plan,indent=2)+'\n')
print('Prepared original-source complete auditors and exact-handle collector for298')
