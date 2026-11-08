from pathlib import Path
import hashlib
import json
import re
import shutil
import xml.etree.ElementTree as ET
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';campaign=root/'build/reader-locale-qualification-298';obs=root/'build/q298-all/20261029T022825Z';source='9fbc9315e8c2417c35492cf505a8f676ecdfcf75'
notes=root.parent/'compilexx/docs/notes/qupathxx-build-inefficiencies.md'
proof=json.loads((campaign/'terminal303-notes-proof.json').read_text(encoding='utf-8'))
assert hashlib.sha256(notes.read_bytes()).hexdigest()==proof['final_sha256']
assert hashlib.sha256(notes.read_bytes()[:proof['prefix_bytes']]).hexdigest()==proof['prefix_sha256']
text=(root/'build/ci/closeout_298_terminal303.py').read_text(encoding='utf-8')
marker="target=work/'docs/benchmarks/reader-locale-qualification-298/terminal303'"
assert text.count(marker)==1
exec(compile(text[text.index(marker):],'closeout_298_terminal303-publication-only','exec'),globals())
