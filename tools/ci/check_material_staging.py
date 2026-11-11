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
import zipfile
from pathlib import PurePosixPath

CATEGORIES = {'admitted_open', 'proprietary', 'noncommercial', 'unreviewed'}
KINDS = {'code', 'source_archive', 'format', 'codec', 'model_weights',
         'training_data', 'evaluation_data', 'asset', 'derived_asset', 'notice'}
LICENSES = {'MIT', 'ISC', 'BSD-2-Clause', 'BSD-3-Clause', 'Apache-2.0', 'Zlib',
            'BSL-1.0', 'OFL-1.1', 'CC0-1.0', 'CC-BY-4.0', 'CC-BY-SA-4.0',
            'GPL-2.0-only', 'GPL-2.0-or-later', 'GPL-3.0-only', 'GPL-3.0-or-later',
            'LGPL-2.1-only', 'LGPL-2.1-or-later', 'LGPL-3.0-only',
            'LGPL-3.0-or-later', 'AGPL-3.0-only', 'AGPL-3.0-or-later', 'Unlicense',
            'FTL', 'blessing', 'libtiff', 'IJG', 'libpng-2.0', 'LGPL-2.0-or-later',
            'BSD-1-Clause', 'BSD-3-Clause AND IJG AND Zlib'}
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


def check_archive(archive, entries, excluded=(), *, expected=None, max_members=100000, max_bytes=8*1024**3):
    """Check every release ZIP/WHL member, never just its container digest.

    Pinned upstream source archives are separately reviewed source artifacts;
    this verifies one explicitly named release-container layer. Release tooling
    must invoke this on each output archive, including the SDK/source companions.
    No extraction, path writes, downloads or license inference occur here.
    """
    path = Path(archive).absolute()
    for part in (path, *path.parents):
        if reparse(part):
            raise ValueError('Archive ancestry contains a link/reparse point: ' + str(part))
    if any(part.casefold() in RESTRICTED_PARTS for part in path.parts):
        raise ValueError('Restricted pool path for archive: ' + str(path))
    if any(path.resolve().is_relative_to(Path(pool).resolve()) for pool in excluded):
        raise ValueError('Archive path resolves outside open pool: ' + str(path))
    by_hash = {}
    for ident, record in entries.items():
        by_hash.setdefault(record['sha256'], []).append(ident)
    seen, files, count, total = set(), set(), 0, 0
    with zipfile.ZipFile(path) as source:
        members = source.infolist()
        if len(members) > max_members:
            raise ValueError('Archive member limit exceeded')
        for member in members:
            # ZipInfo.filename normalizes backslashes on Windows and truncates
            # NUL. Validate the original stored name before that normalization.
            name = member.orig_filename
            pure = PurePosixPath(name)
            if (not name or '\\' in name or '\x00' in name or pure.is_absolute()
                    or '..' in pure.parts or ':' in name or pure.as_posix().rstrip('/') != name.rstrip('/')):
                raise ValueError('Unsafe archive member path: ' + name)
            key = name.rstrip('/').casefold()
            if key in seen:
                raise ValueError('Duplicate/colliding archive member: ' + name)
            seen.add(key)
            if any(part.casefold() in RESTRICTED_PARTS for part in pure.parts):
                raise ValueError('Restricted pool path in archive: ' + name)
            kind = (member.external_attr >> 16) & 0o170000
            if kind not in (0, stat.S_IFREG, stat.S_IFDIR) or member.flag_bits & 1:
                raise ValueError('Linked/special/encrypted archive member: ' + name)
            if member.is_dir():
                continue
            files.add(name)
            if expected is not None and name not in expected:
                raise ValueError('Unexpected archive member: ' + name)
            total += member.file_size
            if member.file_size > 1024**3 or total > max_bytes:
                raise ValueError('Archive byte limit exceeded')
            digest, size = hashlib.sha256(), 0
            with source.open(member) as stream:
                while chunk := stream.read(1024*1024):
                    size += len(chunk)
                    if size > member.file_size:
                        raise ValueError('Archive member size differs: ' + name)
                    digest.update(chunk)
            matches = by_hash.get(digest.hexdigest(), [])
            if not matches:
                raise ValueError('Archive member lacks admitted inventory record: ' + name)
            for ident in matches:
                if entries[ident].get('bytes') != size or size != member.file_size:
                    raise ValueError('Archive member byte size mismatch: ' + name)
                admitted(ident, entries)
            if expected is not None:
                record = entries[expected[name]]
                if record['sha256'] != digest.hexdigest() or record.get('bytes') != size:
                    raise ValueError('Archive member differs from reviewed path identity: ' + name)
                admitted(expected[name], entries)
            count += 1
    if expected is not None and files != set(expected):
        raise ValueError('Archive missing reviewed members: ' + str(sorted(set(expected)-files)))
    if not count:
        raise ValueError('No archive files to check')
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inventory', type=Path, action='append', required=True)
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument('--stage', type=Path)
    target.add_argument('--archive', type=Path)
    parser.add_argument('--exclude-root', type=Path, action='append', default=[])
    args = parser.parse_args()
    try:
        entries = inventory(args.inventory)
        count = check_archive(args.archive, entries, args.exclude_root) if args.archive else check_stage(args.stage, entries, args.exclude_root)
    except (ValueError, OSError, zipfile.BadZipFile) as error:
        parser.exit(1, str(error) + '\n')
    print(json.dumps({'schema': 'qpx.material-staging/1', 'files': count, 'admitted': True}))


if __name__ == '__main__':
    main()
