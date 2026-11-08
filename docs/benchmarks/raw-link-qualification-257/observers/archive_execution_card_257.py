from pathlib import Path
import hashlib
import json
import subprocess

workspace = Path(__file__).resolve().parents[2]
root = workspace / 'build/ci/scientific-grid-159'
source = 'd57df0166888eb335d989d404bdae4d1b63c66a4'
blob = subprocess.check_output(['git', '-C', str(root), 'show', source + ':docs/current-task.md'])
card = blob.split(b'## Preserved local baselines', 1)[0]
history = root / 'docs/project-plan-history.md'
before = history.read_bytes()
addition = b'\n\n## Execution card before candidate257 local acceptance (2026-10-26)\n\n' + card
with history.open('ab') as stream:
    stream.write(addition)
if history.read_bytes() != before + addition:
    raise ValueError('history prefix changed')
proof = dict(source=source, archived_card_bytes=len(card),
             archived_card_sha256=hashlib.sha256(card).hexdigest(),
             history_original_bytes=len(before),
             history_original_sha256=hashlib.sha256(before).hexdigest(), prefix_preserved=True)
target = root / 'docs/benchmarks/raw-link-qualification-257/execution-card-archive.json'
target.write_text(json.dumps(proof, indent=2) + '\n', encoding='utf-8')
print('Archived canonical previous execution card; history prefix preserved.')
