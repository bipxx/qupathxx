"""Compare trusted Mach-O bytes with optional macholib, without loading them.

Accepts native files or wheel members; reads all binaries in memory. This does
not establish loader behavior, package resolution, signatures or release safety.
"""
import argparse
import hashlib
import importlib.metadata
import importlib.util
import io
import json
from pathlib import Path
import zipfile

from macholib.MachO import MachO, lc_str_value


class MemoryMachO(MachO):
    def __init__(self, data):
        self.graphident = self.filename = '<memory>'
        self.loader_path = ''
        self.fat = None
        self.headers = []
        self.allow_unknown_load_commands = False
        self.load(io.BytesIO(data))


def version(value):
    return int(getattr(value, '_version', value))


def compare(data, reader):
    result = reader.inspect_macho(data)
    oracle = MemoryMachO(data)
    assert len(result['slices']) == len(oracle.headers)
    for row, header in zip(result['slices'], oracle.headers):
        assert row['cpu_type'] == int(header.header.cputype) & 0xffffffff
        assert row['cpu_subtype'] == int(header.header.cpusubtype) & 0xffffffff
        assert row['kind'] == reader.KINDS[int(header.header.filetype)]
        assert row['flags'] == int(header.header.flags)
        assert row['offset'] == header.offset and row['bytes'] == header.size
        dependencies, identity, paths, interpreters, minimum = [], None, [], [], []
        for item in header.commands:
            load, command, payload = item
            code = int(load.cmd)
            if code in reader.DYLIBS or code == 0xd:
                record = dict(name=lc_str_value(command.name, item).decode('utf-8'),
                              current_version=version(command.current_version),
                              compatibility_version=version(command.compatibility_version))
                if code == 0xd:identity = record
                else:dependencies.append(dict(mechanism=reader.DYLIBS[code], **record))
            elif code == 0x8000001c:
                paths.append(lc_str_value(command.path, item).decode('utf-8'))
            elif code == 0xe:
                interpreters.append(lc_str_value(command.name, item).decode('utf-8'))
            elif code == 0x24:
                minimum.append(dict(command=code,minimum=version(command.version),sdk=version(command.sdk)))
            elif code == 0x32:
                assert int(command.platform) == 1
                minimum.append(dict(command=code,minimum=version(command.minos),sdk=version(command.sdk)))
        assert [{key:value for key,value in entry.items() if key != 'timestamp'} for entry in row['dependencies']] == dependencies
        assert (None if row['dylib_id'] is None else {key:value for key,value in row['dylib_id'].items() if key != 'timestamp'}) == identity
        assert row['rpaths'] == paths and row['interpreters'] == interpreters
        assert [{key:entry[key] for key in ['command','minimum','sdk']} for entry in row['minimum_os']] == minimum
        assert row['commands'] == [int(load.cmd) for load,command,payload in header.commands]
        assert not any(row[field] for field in ['interpreter_ids','dyld_environment','sub_frameworks','sub_umbrellas','sub_clients','sub_libraries'])
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wheel',type=Path,action='append',default=[])
    parser.add_argument('--file',type=Path,action='append',default=[])
    parser.add_argument('--input-label',required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if not __debug__ or not (args.wheel or args.file) or args.output.exists():
        parser.error('require assertions, trusted inputs and new output')
    source=Path(__file__).with_name('ci')/'macho_dependencies.py'
    spec=importlib.util.spec_from_file_location('qpx_macho_probe',source)
    reader=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reader)
    records=[]
    for path in args.file:
        assert path.stat().st_size<=256*1024*1024
        data=path.read_bytes()
        records.append(dict(input=path.name,sha256=hashlib.sha256(data).hexdigest(),oracle_equal=True,inventory=compare(data,reader)))
    for path in args.wheel:
        assert path.stat().st_size<=1024*1024*1024
        binaries=[]
        with zipfile.ZipFile(path) as archive:
            entries=archive.infolist()
            assert len(entries)<=10000 and sum(entry.file_size for entry in entries)<=512*1024*1024
            for entry in entries:
                if entry.is_dir():continue
                assert entry.file_size<=256*1024*1024
                data=archive.read(entry)
                if data[:4] not in (b'\xcf\xfa\xed\xfe',b'\xca\xfe\xba\xbe',b'\xca\xfe\xba\xbf'):continue
                binaries.append(dict(path=entry.filename,sha256=hashlib.sha256(data).hexdigest(),oracle_equal=True,inventory=compare(data,reader)))
        assert binaries, 'no supported Mach-O images in wheel'
        records.append(dict(input=path.name,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),binaries=binaries))
    report=dict(schema='qpx.macho-reader-oracle/1',input_label=args.input_label,
                parser_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                probe_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                oracle='macholib',oracle_version=importlib.metadata.version('macholib'),records=records,
                extracted=False,binaries_loaded=False,release_ready=False,
                scope='Independent comparison of architecture, file identity, slice bounds, dependency mechanisms/names/versions, '
                      'dylib identity, RPATH/interpreter, minimum macOS/SDK and command IDs. '
                      'No independent timestamp/tool/UUID/segment-payload, package, dyld, symbol/ABI or signing acceptance.')
    with args.output.open('xb') as stream:stream.write((json.dumps(report,indent=2)+'\n').encode('utf-8'))
    print(json.dumps(dict(inputs=len(records),release_ready=False)))


if __name__=='__main__':main()
