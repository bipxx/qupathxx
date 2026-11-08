"""Compile and load only owned fixtures to check PE export-forwarder inventory.

This is a separate Windows compiler probe. It does not load wheel binaries.
The work directory must be new; fixture sources and binaries are retained.
"""
import argparse
import ctypes
import hashlib
import importlib.util
import os
from pathlib import Path
import struct
import subprocess

spec = importlib.util.spec_from_file_location('qpx_runtime_manifest', Path(__file__).with_name('runtime_manifest.py'))
runtime = importlib.util.module_from_spec(spec)
exec(compile(Path(spec.origin).read_bytes(),spec.origin,"exec",dont_inherit=True),runtime.__dict__)


def patch_owned_exports(path, blob_name, alias, forwarded=None):
    raw = bytearray(path.read_bytes())
    image = runtime.pe.Image(bytes(raw))
    directory, _ = image.directory(0)
    header = image.offset(directory, 40)
    functions, names, ordinals = [image.u32(header+at) for at in [28, 32, 36]]
    symbols = {}
    for i in range(image.u32(header+24)):
        name = image.string(image.u32(image.offset(names+i*4, 4)))
        ordinal = image.u16(image.offset(ordinals+i*2, 2))
        symbols[name] = image.u32(image.offset(functions+ordinal*4, 4))
    blob = symbols[blob_name]
    start = image.offset(blob, 512)
    payload = bytearray(512)
    struct.pack_into('<IIHHIIIIIII', payload, 0, 0, 0, 0, 0, blob+0x80, 1, 1, 1, blob+0x40, blob+0x50, blob+0x60)
    struct.pack_into('<I', payload, 0x40, blob+0x140 if forwarded else symbols['actual'])
    struct.pack_into('<I', payload, 0x50, blob+0x100)
    struct.pack_into('<H', payload, 0x60, 0)
    for at, limit, text in [(0x80, 128, path.name), (0x100, 64, alias)]+([(0x140, 192, forwarded)] if forwarded else []):
        encoded = text.encode('ascii')+b'\0'
        runtime.require(len(encoded) <= limit, 'fixture string exceeds its owned storage')
        payload[at:at+len(encoded)] = encoded
    raw[start:start+512] = payload
    optional = struct.unpack_from('<I', raw, 60)[0]+24
    struct.pack_into('<II', raw, optional+112, blob, 512)
    path.write_bytes(raw)


def probe(work, cmake):
    runtime.require(os.name == 'nt', 'native forwarder probe requires Windows')
    work = Path(work).resolve()
    work.mkdir(parents=True, exist_ok=False)
    (work/'bridge.c').write_text('__declspec(dllexport) unsigned char export_blob[512] = {1};\n__declspec(dllexport) int actual(void) {return 123;}\n', encoding='utf-8')
    (work/'front.c').write_text('__declspec(dllexport) unsigned char export_blob[512] = {1};\n__declspec(dllexport) int actual(void) {return 0;}\n', encoding='utf-8')
    (work/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.24)\nproject(qpx_manifest_forwarders C)\nadd_library(bridge SHARED bridge.c)\nadd_library(front SHARED front.c)\nset_target_properties(bridge PROPERTIES OUTPUT_NAME qpx_manifest_bridge)\nset_target_properties(front PROPERTIES OUTPUT_NAME qpx_manifest_front)\n', encoding='utf-8')
    for command in [[str(cmake), '-S', str(work), '-B', str(work/'out'), '-G', 'Visual Studio 17 2022', '-A', 'x64'],
                    [str(cmake), '--build', str(work/'out'), '--config', 'Release', '--parallel', '2']]:
        subprocess.run(command, check=True)
    folder = work/'out/Release'
    bridge, front = folder/'qpx_manifest_bridge.dll', folder/'qpx_manifest_front.dll'
    patch_owned_exports(bridge, 'export_blob', 'sample')
    patch_owned_exports(front, 'export_blob', 'sample', 'qpx_manifest_bridge.sample')
    native = ctypes.WinDLL(str(bridge))
    direct = native.sample
    direct.restype = ctypes.c_int
    forwarding = ctypes.WinDLL(str(front))
    indirect = forwarding.sample
    indirect.restype = ctypes.c_int
    actual, forwarded = direct(), indirect()
    inspected = runtime.pe.inspect_pe(front.read_bytes())
    runtime.require(actual == forwarded == 123 and inspected['export_forwarders'] == ['qpx_manifest_bridge.dll'],
                    'native forwarder dependency or call result differs')
    return dict(schema='qpx.pe-forwarder-native-probe/1', direct_result=actual, forwarded_result=forwarded,
                parser_modules=inspected['export_forwarders'],
                binaries_sha256={path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in [bridge, front]},
                probe_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), fixture=str(work),
                scope='Own C functions and compiler-built fixtures only. Export directory records are written into explicitly allocated fixture storage before loading. Windows resolves the forwarder and returns the independently specified constant. This does not validate arbitrary ABI or loader behavior.', release_ready=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = probe(args.work, args.cmake)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    runtime.publish_new(args.output, runtime.canonical_json(report))
    print('Owned native PE forwarder and dependency inventory passed')


if __name__ == '__main__':
    main()
