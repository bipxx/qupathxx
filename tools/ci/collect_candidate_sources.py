"""Collect explicitly admitted, pinned dependency sources; validate before handoff.

Development/release tooling only. No moving tag lookup, license inference,
restricted material download, product analysis or native build occurs here.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import urllib.request
import zipfile
import check_material_staging as admission

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def generated_record(path, parents):
    digest = sha(path)
    return {'filename': path.name, 'source': 'tools/ci/collect_candidate_sources.py; generated source handoff metadata',
            'bytes': path.stat().st_size, 'sha256': digest, 'material': {
                'id': 'source-handoff-metadata-' + digest, 'category': 'admitted_open',
                'kind': 'derived_asset', 'license_id': 'AGPL-3.0-only',
                'license_evidence': 'LICENSE', 'attribution': 'qupathxx contributors', 'source_ids': list(parents)}}


def collect(output, caches, inventory_paths, profile_name, source_id, allow_download=False):
    if not re.fullmatch('[0-9a-f]{64}', source_id):
        raise ValueError('Source handoff requires the exact compiled source SHA256')
    entries = admission.inventory(inventory_paths)
    profiles = {}
    for path in inventory_paths:
        for name, profile in json.loads(Path(path).read_text(encoding='utf8')).get('release_profiles', {}).items():
            if name in profiles:
                raise ValueError('Duplicate release profile: ' + name)
            profiles[name] = profile
    profile = profiles[profile_name]
    if profile.get('compiled_source_sha256', source_id) != source_id:
        raise ValueError('Dependency companion source identity differs from reviewed profile')
    selected = profile['sources']
    if not selected or len(selected) != len(set(selected)):
        raise ValueError('Source profile needs a finite unique source set')
    filenames = set()
    for ident in selected:
        admission.admitted(ident, entries)
        record = entries[ident]
        if record['material']['kind'] != 'source_archive' or not record.get('review'):
            raise ValueError('Source archive needs an explicit contents/license review: ' + ident)
        filename = Path(record['filename']).name.casefold()
        if filename in filenames:
            raise ValueError('Duplicate/colliding source archive filename: ' + filename)
        filenames.add(filename)
    admission.admitted(profile['firstparty_license'], entries)
    output = Path(output)
    output.mkdir(parents=True, exist_ok=False)
    stage = output/'sources'
    stage.mkdir()
    sources = []
    for ident in selected:
        record = entries[ident]
        filename = Path(record['filename']).name
        candidates = [Path(cache)/filename for cache in caches]
        path = next((p for p in candidates if p.is_file()), None)
        if path is None:
            if record.get('member_selection'):
                raise ValueError('Reviewed code-only source absent; upstream carrier fallback forbidden: ' + filename)
            if not allow_download:
                raise ValueError('Reviewed source absent from cache (download disabled): ' + filename)
            path = candidates[0]
            path.parent.mkdir(parents=True, exist_ok=True)
            # Only explicitly admitted URLs reach this branch. An incomplete
            # transfer stays visible and cannot become a passing cached source.
            partial = path.with_name(path.name + '.partial')
            with urllib.request.urlopen(urllib.request.Request(record.get('url', record['source']),
                    headers={'User-Agent': 'qupathxx-source-handoff'}), timeout=60) as stream, partial.open('xb') as target:
                shutil.copyfileobj(stream, target, 1024*1024)
            if sha(partial) != record['sha256'] or partial.stat().st_size != record['bytes']:
                raise ValueError('Downloaded source identity differs: ' + filename)
            partial.rename(path)
        if path.stat().st_size != record['bytes'] or sha(path) != record['sha256']:
            raise ValueError('Cached source identity differs: ' + filename)
        shutil.copyfile(path, stage/filename)
        sources.append({k: record[k] for k in ('component', 'version', 'sha256', 'bytes')})
        sources[-1].update(filename=filename, url=record.get('url', record['source']), material_id=ident,
                           license_id=record['material']['license_id'], component_notice_ids=record['material']['source_ids'])
    (stage/'sources.json').write_text(json.dumps({'source_sha256': source_id, 'profile': profile_name,
        'sources': sources}, indent=2) + '\n', encoding='utf8')
    (stage/'README.txt').write_text('Pinned dependency sources for native source ' + source_id + '.\n'
        'Reviewed source artifact bytes and individual upstream notices are retained.\n'
        'The application source kit supplies CMake/build recipes and source selection.\n'
        'Only explicitly reviewed source artifacts are collected; no training data or model download is authorized.\n'
        'MSVC/Windows/OpenGL driver remain external prerequisites. Optional Python products are separate.\n', encoding='utf8')
    metadata = [generated_record(stage/name, [profile['firstparty_license']]) for name in ('sources.json', 'README.txt')]
    for record in metadata:
        entries[record['material']['id']] = record
    count = admission.check_stage(stage, entries)
    archive = output/'qupathxx-dependency-sources.zip'
    with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_STORED) as target:
        for path in sorted(stage.iterdir()):
            target.write(path, path.name)
    expected = {item['filename']:item['material_id'] for item in sources}
    expected.update({item['filename']:item['material']['id'] for item in metadata})
    if admission.check_archive(archive, entries, expected=expected) != count:
        raise ValueError('Source archive membership differs from stage')
    metadata_path = output/'generated-materials.json'
    metadata_path.write_text(json.dumps({'records': metadata}, indent=2) + '\n', encoding='utf8')
    result = {'state': 'passed', 'archive': archive.name, 'sha256': sha(archive),
              'bytes': archive.stat().st_size, 'source_sha256': source_id, 'sources': sources,
              'stage_files': count, 'archive_members': count, 'profile': profile_name}
    (output/'dependency-sources.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--extra-cache', type=Path, action='append', default=[])
    parser.add_argument('--inventory', type=Path, action='append')
    parser.add_argument('--profile', default='windows-native')
    parser.add_argument('--source-id', required=True)
    parser.add_argument('--allow-download', action='store_true')
    args = parser.parse_args()
    result = collect(args.output, [args.cache]+args.extra_cache,
        args.inventory or [ROOT/'tests/fixtures/manifest.json'], args.profile, args.source_id, args.allow_download)
    print(json.dumps({k: result[k] for k in ('state','sha256','bytes','stage_files','source_sha256')}, indent=2))


if __name__ == '__main__':
    main()
