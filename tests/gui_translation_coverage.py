"""Exact per-surface untranslated-caption ratchet for localized native draws."""
import json
import re
import shlex
from pathlib import Path


def compare(expected, observed):
    failures = []
    for surface, keys in expected.items():
        if surface not in observed:
            failures.append(f'{surface}: localized surface was not measured')
            continue
        added = set(observed[surface]) - set(keys)
        removed = set(keys) - set(observed[surface])
        if added:
            failures.append(f'{surface}: new untranslated caption: {sorted(added)!r}')
        if removed:
            failures.append(f'{surface}: lower the fixture: {sorted(removed)!r}')
    for surface in observed.keys() - expected.keys():
        if not surface.startswith('tool/'):
            failures.append(f'{surface}: new in-scope surface needs a fixture')
    return failures


def catalogue_keys(source):
    return {json.loads(m[1]) for m in re.finditer(r'^\s*\{("(?:\\.|[^"\\])*")\s*,', source, re.M)}


def check_catalogue(expected, keys):
    return [f'{surface}: lower the fixture: {key!r} now has a catalogue row'
            for surface, missing in expected.items() for key in missing if key in keys]


def load_fixture(root):
    return json.loads((Path(root)/'tests/fixtures/gui-translation-misses.json').read_text(encoding='utf8'))


def read_traces(paths):
    observed = {}
    for path in paths:
        values = shlex.split(Path(path).read_text(encoding='utf8'))
        if len(values) % 2:
            raise ValueError(f'Incomplete translation observation: {path}')
        for surface, key in zip(values[::2], values[1::2]):
            observed.setdefault(surface, set())
            if key:
                observed[surface].add(key)
    return observed


def main():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--trace-dir', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    expected = load_fixture(root)
    failures = compare(expected, read_traces(sorted(args.trace_dir.glob('*.tsv'))))
    failures += check_catalogue(expected, catalogue_keys((root/'cpp/gui_i18n_catalog.inc').read_text(encoding='utf8')))
    for failure in failures:
        print(failure)
    if not failures:
        print('Translation coverage ratchet passed')
    return int(bool(failures))


if __name__ == '__main__':
    raise SystemExit(main())
