from pathlib import Path
import json
root=Path(__file__).resolve().parents[2];ci=root/'build/ci';out=root/'build/original-source-cell-qualification-306'
state=json.loads((out/'campaign.json').read_text(encoding='utf-8'))
assert state['source']=='701fa672db3745aebc7fc5d71108b2c8780cbb6e' and set(state['roles'])=={'engine','macos','portable'}
assert all(v['state']=='run_identified' for v in state['roles'].values())
text=(ci/'collect_candidate298.py').read_text(encoding='utf-8').replace('reader-locale-qualification-298','original-source-cell-qualification-306').replace('candidate298','candidate306').replace('q298-all','q306-all')
with (ci/'collect_candidate306.py').open('x',encoding='utf-8') as stream:stream.write(text)
plan=json.loads((root/'build/reader-locale-qualification-298/complete-audit-plan.json').read_text(encoding='utf-8'))
old={'engine':'40792323979','macos':'40792333489','portable':'40792343129'}
for item in plan:
    role=item['role'];item['cwd']=str(ci/'original-source-cell-qualification-306-audit')
    item['argv']=[v.replace('reader-locale-qualification-298','original-source-cell-qualification-306').replace('q298-all','q306-all').replace('9fbc9315e8c2417c35492cf505a8f676ecdfcf75',state['source']).replace(old[role],str(state['roles'][role]['run']['databaseId'])) for v in item['argv']]
    item['requires']='Terminal role plus every applicable authenticated archive; frozen306 original complete auditors only'
with (out/'complete-audit-plan.json').open('x',encoding='utf-8') as stream:json.dump(plan,stream,indent=2);stream.write('\n')
print('Prepared original306 complete auditors and exact-handle collector')
