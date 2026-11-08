"""Observe linked-image serialization under two spellings of one output root."""
from pathlib import Path
import hashlib
import json
import sqlite3
import qupathxx as q

ws = Path(__file__).resolve().parents[2]
out = ws / 'build/root-alias-analysis-259'
out.mkdir(exist_ok=False)
source = out / 'source.ppm'
source.write_bytes(b'P6\n2 2\n255\n' + bytes([30, 60, 90]) * 4)
original = out / 'original.qpxproj'
project = q.Project()
project.add_image(str(source), 'ppm')
project.save(str(original))
directory = out / 'snapshots'
directory.mkdir()
plain = directory / 'plain.qpxproj'
extended = directory / 'extended.qpxproj'
text = str(extended)
extended_argument = '\\\\?\\UNC\\' + text[2:] if text.startswith('\\\\') else '\\\\?\\' + text
first = q.Project.open(str(original))
second = q.Project.open(str(original))
first.save_new(str(plain))
second.save_new(extended_argument)


def rows(path):
    with sqlite3.connect(path) as db:
        tables = [r[0] for r in db.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")]
        return {name: list(db.execute('SELECT * FROM "' + name.replace('"', '""') + '"')) for name in tables}


a, b = rows(plain), rows(extended)
changed = [name for name in a if a[name] != b[name]]
ordinary_source, extended_source = a['images'][0][3], b['images'][0][3]
normalized = dict(b)
normalized['images'] = [tuple(list(row[:3]) + [a['images'][i][3]] + list(row[4:])) for i, row in enumerate(b['images'])]
report = dict(source=q.build_environment().git_revision,
    native=str(q._native.__file__), changed_tables=changed,
    ordinary_image_locator=ordinary_source, extended_image_locator=extended_source,
    equal_after_only_locator_normalization=a == normalized,
    source_file_unchanged=hashlib.sha256(source.read_bytes()).hexdigest(),
    ordinary_snapshot_sha256=hashlib.sha256(plain.read_bytes()).hexdigest(),
    extended_snapshot_sha256=hashlib.sha256(extended.read_bytes()).hexdigest(),
    same_directory=Path(extended_argument).parent.samefile(directory),
    same_image=(directory / ordinary_source).samefile(directory / extended_source),
    scope='Public snapshot API serialization witness only; no repair or qualification')
(out / 'observation.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print(json.dumps(report, indent=2))
