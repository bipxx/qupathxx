"""Offline admission guard over existing inventories and exact staged file bytes.

Records retain their existing sha256/bytes/url/license fields and add a material
object: id, category, kind, license_id, license_evidence, attribution, source_ids.
No download, license inference, or private-user-data scanning occurs here.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import stat

CATEGORIES = {'admitted_open', 'proprietary', 'noncommercial', 'unreviewed'}
KINDS = {'code', 'source_archive', 'format', 'codec', 'model_weights',
         'training_data', 'evaluation_data', 'asset', 'derived_asset', 'notice'}
LICENSES = {'MIT', 'ISC', 'BSD-2-Clause', 'BSD-3-Clause', 'Apache-2.0', 'Zlib',
            'BSL-1.0', 'OFL-1.1', 'CC0-1.0', 'CC-BY-4.0', 'CC-BY-SA-4.0',
            'GPL-2.0-only', 'GPL-2.0-or-later', 'GPL-3.0-only', 'GPL-3.0-or-later',
            'LGPL-2.1-only', 'LGPL-2.1-or-later', 'LGPL-3.0-only',
            'LGPL-3.0-or-later', 'AGPL-3.0-only', 'AGPL-3.0-or-later', 'Unlicense'}
RESTRICTED_PARTS = {'qupathxx-restricted', 'proprietary', 'noncommercial', 'unreviewed'}


def records(value):
    if isinstance(value, dict):
        if 'material' in value:
            yield value
        for key, child in value.items():
            if key != 'material':
                yield from records(child)
    elif isinstance(value, list):
        for child in value:
            yield from records(child)


def inventory(paths):
    result = {}
    for path in paths:
        for record in records(json.loads(Path(path).read_text(encoding='utf-8'))):
            material = record['material']
            if not isinstance(material, dict):
                raise ValueError('Material declaration must be an object')
            ident = material.get('id')
            if not isinstance(ident, str) or not ident or ident in result:
                raise ValueError('Missing or duplicate material ID: ' + str(ident))
            if material.get('category') not in CATEGORIES or material.get('kind') not in KINDS:
                raise ValueError('Unknown material category/kind: ' + ident)
            digest = record.get('sha256', '')
            if not re.fullmatch('[0-9a-f]{64}', digest):
                raise ValueError('Material needs an exact SHA256: ' + ident)
            size = record.get('bytes')
            if size is not None and (type(size) is not int or size < 0):
                raise ValueError('Invalid material byte size: ' + ident)
            parents = material.get('source_ids')
            if not isinstance(parents, list) or any(not isinstance(p, str) for p in parents) or len(parents) != len(set(parents)):
                raise ValueError('Material needs unique source_ids: ' + ident)
            if material['kind'] == 'derived_asset' and not parents:
                raise ValueError('Derived material needs source lineage: ' + ident)
            result[ident] = record
    return result


def admitted(ident, entries, visiting=None, checked=None):
    visiting = set() if visiting is None else visiting
    checked = set() if checked is None else checked
    if ident in checked:
        return
    if ident in visiting or ident not in entries:
        raise ValueError('Cyclic or missing material source: ' + ident)
    record = entries[ident]
    material = record['material']
    if material['category'] != 'admitted_open':
        raise ValueError('Restricted/unreviewed material cannot be staged: ' + ident)
    for key in ('license_id', 'license_evidence', 'attribution'):
        if not isinstance(material.get(key), str) or not material[key].strip():
            raise ValueError('Missing material ' + key + ': ' + ident)
    # Admission is explicit and evidenced; free-download/unknown/NC strings do
    # not become open merely because a record accidentally says admitted_open.
    if material['license_id'] not in LICENSES:
        raise ValueError('Unreviewed license expression: ' + ident)
    if not record.get('url') and not record.get('source'):
        raise ValueError('Missing exact material source reference: ' + ident)
    for reference in (record.get('url', ''), record.get('source', ''), material['license_evidence']):
        if any(part.casefold() in RESTRICTED_PARTS for part in re.split(r'[\\/]+', reference)):
            raise ValueError('Restricted source reference cannot be admitted: ' + ident)
    visiting.add(ident)
    for parent in material['source_ids']:
        admitted(parent, entries, visiting, checked)
    visiting.remove(ident)
    checked.add(ident)


def reparse(path):
    info = path.lstat()
    return path.is_symlink() or bool(getattr(info, 'st_file_attributes', 0) & getattr(stat, 'FILE_ATTRIBUTE_REPARSE_POINT', 0x400))


def check_stage(stage, entries, excluded=()):
    stage = Path(stage).absolute()
    # Resolve ancestry too: a stage rooted below a junction must not redirect to
    # an external restricted pool before the first directory is enumerated.
    for part in (stage, *stage.parents):
        if reparse(part):
            raise ValueError('Staging ancestry contains a link/reparse point: ' + str(part))
    root = stage.resolve(strict=True)
    excluded = [Path(path).resolve() for path in excluded]
    by_hash = {}
    for ident, record in entries.items():
        by_hash.setdefault(record['sha256'], []).append(ident)
    count = 0
    def visit(path):
        nonlocal count
        if any(part.casefold() in RESTRICTED_PARTS for part in path.parts):
            raise ValueError('Restricted pool path in stage: ' + str(path))
        if reparse(path):
            raise ValueError('Staged link/reparse point is not an admitted file: ' + str(path))
        resolved = path.resolve(strict=True)
        if not resolved.is_relative_to(root) or any(resolved.is_relative_to(pool) for pool in excluded):
            raise ValueError('Staged path resolves outside open pool: ' + str(path))
        if path.is_dir():
            for child in sorted(path.iterdir()):
                visit(child)
            return
        if not path.is_file():
            raise ValueError('Unsupported staged filesystem object: ' + str(path))
        with path.open('rb') as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        matches = by_hash.get(digest, [])
        if not matches:
            raise ValueError('Staged file lacks admitted inventory record: ' + str(path))
        for ident in matches:
            if entries[ident].get('bytes') != path.stat().st_size:
                raise ValueError('Staged byte size mismatch: ' + ident)
            admitted(ident, entries)
        count += 1
    visit(root)
    if not count:
        raise ValueError('No staged files to check')
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inventory', type=Path, action='append', required=True)
    parser.add_argument('--stage', type=Path, required=True)
    parser.add_argument('--exclude-root', type=Path, action='append', default=[])
    args = parser.parse_args()
    try:
        count = check_stage(args.stage, inventory(args.inventory), args.exclude_root)
    except (ValueError, OSError) as error:
        parser.exit(1, str(error) + '\n')
    print(json.dumps({'schema': 'qpx.material-staging/1', 'files': count, 'admitted': True}))


if __name__ == '__main__':
    main()
