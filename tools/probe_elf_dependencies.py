"""Compare trusted wheel ELF declarations with optional pyelftools (no loading).

Run in an isolated environment with pyelftools installed. Retains per-file hashes
and observations in a new JSON file. This is not package/loader acceptance.
"""
import argparse
import hashlib
import importlib.metadata
import importlib.util
import io
import json
from pathlib import Path
import re
import zipfile

from elftools.elf.elffile import ELFFile


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wheel', type=Path, action='append', required=True)
    parser.add_argument('--package-source', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if not __debug__ or not re.fullmatch('[0-9a-f]{40}', args.package_source) or args.output.exists():
        parser.error('require assertions, full trusted package source commit and new output')
    source = Path(__file__).with_name('ci')/'elf_dependencies.py'
    spec = importlib.util.spec_from_file_location('qpx_elf_probe', source)
    reader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reader)
    records = []
    for path in args.wheel:
        assert path.stat().st_size <= 1024*1024*1024
        binaries = []
        with zipfile.ZipFile(path) as archive:
            entries = archive.infolist()
            assert len(entries) <= 10000 and sum(item.file_size for item in entries) <= 512*1024*1024
            for entry in entries:
                if entry.is_dir():
                    continue
                assert entry.file_size <= 256*1024*1024
                data = archive.read(entry)
                if not data.startswith(b'\x7fELF'):
                    continue
                actual = reader.inspect_elf(data)
                oracle = ELFFile(io.BytesIO(data))
                assert oracle.elfclass == 64 and oracle.little_endian and oracle.header.e_machine == 'EM_X86_64'
                dynamic = next(segment for segment in oracle.iter_segments() if segment.header.p_type == 'PT_DYNAMIC')
                tags = list(dynamic.iter_tags())
                needed = [tag.needed for tag in tags if tag.entry.d_tag == 'DT_NEEDED']
                def optional(tag_name, attribute):
                    values = [getattr(tag, attribute) for tag in tags if tag.entry.d_tag == tag_name]
                    assert len(values) <= 1
                    return values[0] if values else None
                rpath, runpath = optional('DT_RPATH', 'rpath'), optional('DT_RUNPATH', 'runpath')
                interpreter = next((segment.get_interp_name() for segment in oracle.iter_segments()
                                    if segment.header.p_type == 'PT_INTERP'), None)
                expected = dict(needed=needed, soname=optional('DT_SONAME', 'soname'),
                                rpath=None if rpath is None else rpath.split(':'),
                                runpath=None if runpath is None else runpath.split(':'), interpreter=interpreter)
                assert {key: actual[key] for key in expected} == expected, (entry.filename, actual, expected)
                # These uncommon tags are covered by independent byte fixtures;
                # this real-package probe currently requires their absence.
                assert all(actual[field] in ([], None) for field in ['auxiliary', 'filters', 'audit', 'depaudit'])
                versions = []
                section = oracle.get_section_by_name('.gnu.version_r')
                if section is not None:
                    for version, auxiliaries in section.iter_versions():
                        versions.append(dict(file=version.name, versions=[dict(name=aux.name,
                            hash=aux['vna_hash'], flags=aux['vna_flags'], index=aux['vna_other']) for aux in auxiliaries]))
                assert actual['version_requirements'] == versions, entry.filename
                binaries.append(dict(path=entry.filename, sha256=sha(data), oracle_equal=True, inventory=actual))
        assert binaries, 'wheel has no ELF images'
        records.append(dict(archive=path.name, archive_sha256=sha(path.read_bytes()), binaries=binaries))
    report = dict(schema='qpx.elf-dependency-oracle/1', parser_sha256=sha(source.read_bytes()),
                  probe_sha256=sha(Path(__file__).read_bytes()), input_source=args.package_source,
                  oracle='pyelftools', oracle_version=importlib.metadata.version('pyelftools'), records=records,
                  package_binaries_extracted=False, package_binaries_loaded=False, release_ready=False,
                  scope='Dynamic names, search/interpreter fields and version requirements only. '
                        'No symbol resolution, package resolution, loader, prerequisite availability or release acceptance.')
    with args.output.open('xb') as stream:
        stream.write((json.dumps(report, indent=2)+'\n').encode('utf-8'))
    print(json.dumps(dict(wheels=len(records), binaries=sum(len(record['binaries']) for record in records))))


if __name__ == '__main__':
    main()
