#!/usr/bin/env python3
"""Development-only authored QOI fixtures, pinned reference oracle and native checks."""
import argparse
from decimal import Decimal, localcontext
import hashlib
import json
import os
from pathlib import Path
import random
import shutil
import struct
import subprocess

END = b"\x00" * 7 + b"\x01"


def qoi(width, height, chunks, channels=4, color=0):
    return struct.pack(">4sIIBB", b"qoif", width, height, channels, color) + bytes(chunks) + END


def explicit(values):
    return b"".join(b"\xff" + bytes(values[i:i + 4]) for i in range(0, len(values), 4))


def display(raw, linear):
    if not linear:
        return bytes(raw)
    with localcontext() as context:
        context.prec = 60
        lut = []
        for i in range(256):
            value = Decimal(i) / 255
            converted = value * Decimal('12.92') if value <= Decimal('0.0031308') else (
                Decimal('1.055') * value ** (Decimal(1) / Decimal('2.4')) - Decimal('0.055'))
            lut.append(int((converted * 255).to_integral_value(rounding='ROUND_HALF_UP')))
    return bytes(lut[v] if i % 4 != 3 else v for i, v in enumerate(raw))


def write_fixtures(directory, reference):
    directory.mkdir(parents=True, exist_ok=True)
    cases, negative = [], {}

    def add(name, width, height, values, chunks=None, *, channels=4, color=0, encoded=None):
        raw = bytes(values)
        assert len(raw) == width * height * 4
        data = encoded if encoded is not None else qoi(width, height, explicit(raw) if chunks is None else chunks, channels, color)
        path = directory / (name + '.qoi')
        path.write_bytes(data)
        (directory / (name + '.raw')).write_bytes(raw)
        rgba = display(raw, color)
        (directory / (name + '.rgba')).write_bytes(rgba)
        oracle = directory / (name + '.reference.raw')
        r = subprocess.run([str(reference), 'decode', str(path), str(oracle)], capture_output=True, text=True, check=True)
        assert oracle.read_bytes() == raw, (name, 'reference decoder differs from authored codes')
        assert r.stdout.split() == [str(width), str(height), str(channels), str(color)], name
        cases.append({'name': name, 'width': width, 'height': height, 'channels': 4,
                      'vendor': 'QOI-' + ('linear' if color else 'sRGB') + ('-RGB' if channels == 3 else '-RGBA'),
                      'header_channels': channels, 'colorspace': color,
                      'source_sha256': hashlib.sha256(data).hexdigest(), 'raw_sha256': hashlib.sha256(raw).hexdigest(),
                      'rgba_sha256': hashlib.sha256(rgba).hexdigest(), 'reference_exact': True})
        return data

    opaque = bytes(v for i in range(15) for v in ((i * 17 + 3) & 255, (i * 53 + 9) & 255, (i * 71 + 21) & 255, 255))
    base = add('rgb-main', 5, 3, opaque)
    alpha = bytes(v for i in range(15) for v in ((i * 31 + 13) & 255, (i * 43 + 7) & 255, (i * 67 + 19) & 255, (0, 1, 127, 254, 255)[i % 5]))
    add('alpha-main', 5, 3, alpha)
    add('rgb-hint-alpha', 5, 3, alpha, channels=3)
    linear = bytes(v for i in range(15) for v in ((0, 1, 2, 10, 31, 64, 128, 192, 254, 255)[i % 10], (i * 17) & 255, (i * 53) & 255, 255))
    add('linear-main', 5, 3, linear, color=1)
    add('linear-hint-alpha', 5, 3, alpha, channels=3, color=1)
    all_codes = bytes(v for i in range(256) for v in (i, 255 - i, i, (i * 37) & 255))
    add('linear-all-codes', 256, 1, all_codes, color=1)
    changed = bytearray(opaque); changed[0] ^= 1
    add('rgb-source-changed', 5, 3, changed)
    add('initial-index', 64, 1, bytes(64 * 4), bytes(range(64)))
    add('initial-run', 2, 1, bytes((0, 0, 0, 255)) * 2, bytes((0xc0, 53)))
    add('rgb-preserves-alpha', 3, 1, bytes((10, 20, 30, 13, 31, 41, 51, 13, 61, 71, 81, 13)),
        bytes((0xff, 10, 20, 30, 13, 0xfe, 31, 41, 51, 0xfe, 61, 71, 81)))
    chunks, expected = bytearray(), bytearray()
    for tag in range(0x40, 0x80):
        seed = (253, 0, 255, 17)
        chunks += b'\xff' + bytes(seed) + bytes((tag,))
        expected += bytes(seed) + bytes(((seed[0] + ((tag >> 4) & 3) - 2) & 255,
                                         (seed[1] + ((tag >> 2) & 3) - 2) & 255,
                                         (seed[2] + (tag & 3) - 2) & 255, seed[3]))
    add('diff-all', 128, 1, expected, chunks)
    chunks, expected = bytearray(), bytearray()
    for green in range(-32, 32):
        for red in range(-8, 8):
            for blue in range(-8, 8):
                seed = (0, 254, 1, 61)
                chunks += b'\xff' + bytes(seed) + bytes((0x80 | (green + 32), ((red + 8) << 4) | (blue + 8)))
                expected += bytes(seed) + bytes(((seed[0] + green + red) & 255,
                                                 (seed[1] + green) & 255, (seed[2] + green + blue) & 255, seed[3]))
    add('luma-all', len(expected) // 4, 1, expected, chunks)
    chunks, expected = bytearray(), bytearray()
    for count in range(1, 63):
        seed = (count, 2 * count, 3 * count, count)
        chunks += b'\xff' + bytes(seed) + bytes((0xc0 | (count - 1),))
        expected += bytes(seed) * (count + 1)
    add('run-all', len(expected) // 4, 1, expected, chunks)
    colors = [(((slot - 53) * 43) % 64, 0, 0, 255) for slot in range(64)]
    chunks = b''.join(b'\xff' + bytes(c) for c in colors) + bytes(range(64))
    add('index-all', 128, 1, bytes(v for c in colors + colors for v in c), chunks)
    first, second = (1, 2, 3, 4), (65, 2, 3, 4)
    slot = (first[0] * 3 + first[1] * 5 + first[2] * 7 + first[3] * 11) % 64
    add('index-collision', 3, 1, bytes(first + second + second), b'\xff' + bytes(first) + b'\xff' + bytes(second) + bytes((slot,)))
    for width, height in ((1, 1), (3, 2), (17, 5), (127, 3)):
        for hint in (3, 4):
            for color in (0, 1):
                values = bytes(v for i in range(width * height) for v in ((i * 29 + 9) & 255, (i * 59) & 255, (i * 107 + 13) & 255, (i * 73) & 255))
                add(f'shape-{width}-{height}-h{hint}-c{color}', width, height, values, channels=hint, color=color)
    total = 256 * 256
    chunks = bytes([0xfd]) * (total // 62)
    if total % 62: chunks += bytes((0xc0 | ((total % 62) - 1),))
    add('budget-raster', 256, 256, bytes((0, 0, 0, 255)) * total, chunks)
    rng = random.Random(720251105)
    for i in range(8):
        width, height = 11 + i, 7
        values = bytes(rng.randrange(256) for _ in range(width * height * 4))
        raw = directory / f'encoded-{i}.input.raw'; raw.write_bytes(values)
        encoded = directory / f'encoded-{i}.qoi'
        subprocess.run([str(reference), 'encode', str(raw), str(width), str(height), '4', str(i % 2), str(encoded)], check=True, capture_output=True)
        add(f'encoded-{i}', width, height, values, encoded=encoded.read_bytes(), color=i % 2)

    def reject(name, data, status=4):
        (directory / (name + '.qoi')).write_bytes(data); negative[name] = status
    reject('not-qoi', b'PNG is not QOI', 2)
    reject('short-header', b'qoif')
    for at, value in ((4, 0), (8, 0)):
        data = bytearray(base); data[at:at + 4] = bytes(4); reject(f'zero-dimension-{at}', data)
    for hint in (0, 1, 2, 5, 255):
        data = bytearray(base); data[12] = hint; reject(f'bad-channel-{hint}', data, 2)
    data = bytearray(base); data[13] = 2; reject('bad-colorspace', data, 2)
    data = bytearray(base); data[-1] = 0; reject('bad-end-marker', data)
    reject('trailing-byte', base + b'x')
    reject('concatenated', base + base)
    reject('extra-chunk', base[:-8] + b'\xc0' + END)
    reject('no-pixels', qoi(1, 1, b''))
    reject('run-overflow', qoi(1, 1, b'\xfd'))
    reject('repeat-index', qoi(2, 1, b'\0\0'))
    for size in range(4): reject(f'truncated-rgba-{size}', qoi(1, 1, b'\xff' + bytes(range(size))))
    for size in range(3): reject(f'truncated-rgb-{size}', qoi(1, 1, b'\xfe' + bytes(range(size))))
    reject('truncated-luma', qoi(1, 1, b'\x80'))
    reject('giant-raster', qoi(0xffffffff, 0xffffffff, b'\xc0'), 3)
    shutil.copyfile(directory / 'rgb-main.qoi', directory / 'QOI-\u03bc-\u56fe.qoi')
    (directory / 'fixtures.tsv').write_text(''.join(f"{c['name']} {c['width']} {c['height']} 4 {c['vendor']} 0 0 0\n" for c in cases), encoding='utf-8')
    (directory / 'negative.tsv').write_text(''.join(f'{name} {status}\n' for name, status in negative.items()), encoding='utf-8')
    (directory / 'counts.txt').write_text(f'{len(cases)} {len(negative)}\n', encoding='utf-8')
    report = {'schema': 'qpx.qoi-authored-controls/1', 'positive': cases, 'negative': negative,
              'reference': str(reference), 'oracle_scope': 'Exact literal RGBA codes with separately pinned MIT reference decoder/encoder; Decimal60 display oracle',
              'external_images': False, 'fixture_license': 'AGPL-3.0-only; independently authored qupathxx controls'}
    (directory / 'fixtures.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return report


def numpy_payload(path, shape, descriptor):
    # Independent standard-library inspection; no installed NumPy is required.
    import ast
    data = path.read_bytes()
    assert data[:8] == b'\x93NUMPY\x01\x00', path
    length = struct.unpack('<H', data[8:10])[0]
    header = ast.literal_eval(data[10:10 + length].decode('ascii').strip())
    assert header == {'descr': descriptor, 'fortran_order': False, 'shape': tuple(shape)}, (path, header)
    return data[10 + length:]


def validate_peer_raw_files(region, manifest, output, run):
    """Missing consumer routes only; reuse the exact accepted codec fixtures."""
    root = Path(__file__).resolve().parents[2]
    entries = json.loads(manifest.read_text(encoding='utf-8'))['plugins']
    result = []
    for family in ('jpeg', 'pnm', 'bmp', 'webp', 'nrrd', 'ics'):
        provider = 'org.qupathxx.' + family
        entry = next(e for e in entries if e['provider_id'] == provider)
        selected = None
        for evidence in (root / 'build' / (family + '-reader')).glob('**/validation.json'):
            try:
                prior = json.loads(evidence.read_text(encoding='utf-8'))
            except (ValueError, OSError):
                continue
            fixture_meta = evidence.parent / 'fixtures/fixtures.json'
            if prior.get('passed') and prior.get('module_sha256') == entry['module']['sha256'] and fixture_meta.is_file():
                selected = (evidence, fixture_meta)
                break
        if selected is None:
            raise ValueError('Need exact admitted fixture evidence for ' + provider)
        evidence, fixture_meta = selected
        case = json.loads(fixture_meta.read_text(encoding='utf-8'))['cases'][0]
        suffix = {'jpeg': '.jpg', 'webp': '.webp', 'bmp': '.bmp', 'nrrd': '.nrrd', 'ics': '.ics'}
        filename = case.get('filename', case['name'] + suffix.get(family, '.pnm'))
        source = fixture_meta.parent / filename
        expected = (fixture_meta.parent / (case['name'] + '.raw')).read_bytes()
        assert hashlib.sha256(expected).hexdigest() == case['raw_sha256']
        channels, width, height = case.get('channels', 1), case['width'], case['height']
        sample = case.get('sample_type', 2 if case.get('sample_bytes') == 2 else 1)
        descriptors = {1: '|u1', 2: '<u2', 3: '|i1', 4: '<i2', 5: '<u4', 6: '<i4', 7: '<u8', 8: '<i8', 9: '<f4', 10: '<f8'}
        size = {1: 1, 2: 2, 3: 1, 4: 2, 5: 4, 6: 4, 7: 8, 8: 8, 9: 4, 10: 8}[sample]
        expected = expected[:width * height * channels * size]
        target = output / ('peer-' + family + '.npy')
        before = hashlib.sha256(source.read_bytes()).hexdigest()
        r = run('raw-peer-' + family, [region, '--reader-plugins', manifest, '--reader', provider,
                                      '--raw', source, target, '--channels', channels])
        assert numpy_payload(target, (height, width, channels), descriptors[sample]) == expected
        snapshot = json.loads(r.stdout)['reader_snapshot']
        assert snapshot['implementation']['module_sha256'] == entry['module']['sha256']
        assert hashlib.sha256(source.read_bytes()).hexdigest() == before
        result.append({'provider_id': provider, 'qualified_evidence': str(evidence), 'fixture': str(source),
                       'source_sha256': before, 'raw_exact': True, 'selected_z': 0})
    return result


def validate(args):
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        raise ValueError('Use a fresh validation directory')
    output.mkdir(parents=True, exist_ok=True)
    fixtures = args.fixtures.resolve()
    controls = json.loads((fixtures / 'fixtures.json').read_text(encoding='utf-8'))
    assert len(controls['positive']) == 40 and len(controls['negative']) == 26
    for case in controls['positive']:
        for suffix, key in (('.qoi', 'source_sha256'), ('.raw', 'raw_sha256'), ('.rgba', 'rgba_sha256')):
            assert hashlib.sha256((fixtures / (case['name'] + suffix)).read_bytes()).hexdigest() == case[key]
    env = {k: v for k, v in os.environ.items() if not k.upper().startswith(('PYTHON', 'VIRTUAL_ENV', 'CONDA'))}
    env['PATH'] = r'C:\Windows\System32;C:\Windows'
    rows = []
    artifacts = {name: getattr(args, name).resolve() for name in ('module', 'abi_test', 'consumer', 'manifest', 'region', 'reference')}
    def hashes():
        return {name: hashlib.sha256(p.read_bytes()).hexdigest() for name, p in artifacts.items()}
    before = hashes()
    report = {'schema': 'qpx.qoi-plugin-validation/1', 'checks': rows, 'fixture_cases': 40, 'negative_cases': 26,
              'artifact_paths': {k: str(v) for k, v in artifacts.items()}, 'artifacts_before': before,
              **{k + '_sha256': v for k, v in before.items()}, 'native_only_environment': True}

    def run(name, command, expected=0):
        result = subprocess.run(list(map(str, command)), cwd=output, env=env, capture_output=True,
                                text=True, encoding='utf-8', errors='replace', timeout=60)
        row = {'name': name, 'argv': list(map(str, command)), 'returncode': result.returncode,
               'stdout': result.stdout, 'stderr': result.stderr}
        rows.append(row)
        (output / (name + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        if result.returncode != expected:
            raise RuntimeError(name + ': ' + result.stdout + result.stderr)
        return result

    try:
        run('direct-abi', [args.abi_test.resolve(), args.module.resolve(), fixtures])
        work = output / 'project'
        for mode in ('create', 'reopen', 'missing', 'changed-source'):
            run('project-' + mode, [args.consumer.resolve(), mode, args.manifest.resolve(), fixtures, work])
        changed = output / 'changed-module'; changed.mkdir()
        changed_manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
        for entry in changed_manifest['plugins']:
            path = changed / entry['module']['path']; path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes((args.manifest.parent / entry['module']['path']).read_bytes() + b'QPX-QOI-IDENTITY-CONTROL')
            entry['module']['sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
        changed_path = changed / 'reader-plugins.json'
        changed_path.write_text(json.dumps(changed_manifest, indent=2) + '\n', encoding='utf-8')
        run('project-changed-implementation', [args.consumer.resolve(), 'changed-implementation', changed_path, fixtures, work])
        prefix = [args.region.resolve(), '--reader-plugins', args.manifest.resolve(), '--reader', 'org.qupathxx.qoi']
        catalog = run('catalog', [args.region.resolve(), '--reader-plugins', args.manifest.resolve(), '--readers'])
        assert 'org.qupathxx.qoi' in catalog.stdout
        for name in ('rgb-main', 'linear-main', 'alpha-main', 'rgb-hint-alpha'):
            pam = output / (name + '.pam')
            run('display-' + name, [*prefix, fixtures / (name + '.qoi'), 0, 0, 0, 5, 3, pam])
            encoded = pam.read_bytes(); end = encoded.index(b'ENDHDR\n') + 7
            assert encoded[end:] == (fixtures / (name + '.rgba')).read_bytes()
            array = output / (name + '.npy')
            result = run('raw-' + name, [*prefix, '--raw', fixtures / (name + '.qoi'), array, '--channels', 4])
            assert numpy_payload(array, (3, 5, 4), '|u1') == (fixtures / (name + '.raw')).read_bytes()
            info = json.loads(result.stdout)
            assert info['schema'] == 'qpx.raw-region/2' and info['valid_pixels'] == 15
            assert info['reader_snapshot']['implementation']['provider_id'] == 'org.qupathxx.qoi'
            assert info['reader_snapshot']['implementation']['module_sha256'] == before['module']
        cropped = output / 'channels.npy'
        run('raw-channel-crop', [*prefix, '--raw', fixtures / 'rgb-main.qoi', cropped, '--channel', 1,
                                '--channels', 2, '--x', 1, '--y', 1, '--width', 2, '--height', 1])
        raw = (fixtures / 'rgb-main.raw').read_bytes()
        assert numpy_payload(cropped, (1, 2, 2), '|u1') == b''.join(raw[(5 + x) * 4 + 1:(5 + x) * 4 + 3] for x in (1, 2))
        source = output / 'protected.qoi'; shutil.copyfile(fixtures / 'rgb-main.qoi', source)
        source_before = source.read_bytes()
        alias = output / 'protected-alias.pam'; os.link(source, alias)
        run('preview-source-alias', [*prefix, source, 0, 0, 0, 5, 3, alias], expected=1)
        assert source.read_bytes() == source_before
        run('raw-source-alias', [*prefix, '--raw', source, alias, '--channels', 4], expected=2)
        existing = output / 'rgb-main.pam'; before_pam = existing.read_bytes()
        run('preview-existing-output', [*prefix, source, 0, 0, 0, 5, 3, existing], expected=1)
        assert existing.read_bytes() == before_pam
        for name, flags in [('bad-z', ['--z', 1]), ('tiff-options', ['--cache-bytes', 0]),
                            ('padding-without-validity', ['--x', -1, '--width', 1, '--height', 1]),
                            ('bad-width', ['--width', 0])]:
            destination = output / (name + '.npy')
            run(name, [*prefix, '--raw', source, destination, '--channels', 4, *flags], expected=2)
            assert not destination.exists()
        destination = output / 'missing-plugin.npy'
        run('raw-missing-plugin', [args.region.resolve(), '--reader', 'org.qupathxx.qoi', '--raw', source, destination, '--channels', 4], expected=2)
        assert not destination.exists() and source.read_bytes() == source_before
        if args.coexist:
            coexist = output / 'coexist'; coexist.mkdir()
            combined = json.loads(args.coexist.read_text(encoding='utf-8'))
            assert len(combined['plugins']) == 7
            for entry in combined['plugins']:
                for item in [entry['module'], *entry['dependencies']]:
                    target = coexist / item['path']; target.parent.mkdir(parents=True, exist_ok=True)
                    source_file = args.coexist.parent / item['path']
                    assert hashlib.sha256(source_file.read_bytes()).hexdigest() == item['sha256']
                    shutil.copyfile(source_file, target)
            for entry in json.loads(args.manifest.read_text(encoding='utf-8'))['plugins']:
                entry['module']['path'] = 'qoi/' + entry['module']['path']
                target = coexist / entry['module']['path']; target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(args.module.resolve(), target); combined['plugins'].append(entry)
            manifest = coexist / 'reader-plugins.json'; manifest.write_text(json.dumps(combined, indent=2) + '\n')
            r = run('eight-reader-catalog', [args.region.resolve(), '--reader-plugins', manifest, '--readers'])
            native_catalog = json.loads(r.stdout)
            admitted = {e['provider_id']: e for e in native_catalog['readers']}
            assert not native_catalog['plugin_diagnostics']
            assert all(admitted[e['provider_id']]['available'] and
                       admitted[e['provider_id']]['module_sha256'] == e['module']['sha256']
                       for e in combined['plugins'])
            report['coexistence_readers'] = [e['provider_id'] for e in combined['plugins']]
            peer = output / 'peer16.png'
            png_controls = next((Path(__file__).resolve().parents[2] / 'build/gui-aggregation/gap-closure/png-labels/checks/png_configured').glob('png-labels-*/gray-16.png'))
            shutil.copyfile(png_controls, peer)
            array = output / 'peer-png.npy'
            run('raw-peer-png', [args.region.resolve(), '--reader-plugins', manifest, '--reader', 'org.qupathxx.png', '--raw', peer, array])
            assert numpy_payload(array, (2, 2, 1), '<u2') == struct.pack('<4H', 65535, 1, 65535, 0)
            report['peer_raw_routes'] = validate_peer_raw_files(args.region.resolve(), manifest, output, run)
        report['checks_passed'] = True
    finally:
        report['artifacts_after'] = hashes()
        report['artifacts_unchanged'] = before == report['artifacts_after']
        report['passed'] = bool(report.get('checks_passed') and report['artifacts_unchanged'])
        (output / 'validation.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'passed': report['passed'], 'native_checks': len(rows), 'fixture_cases': 40, 'negative_cases': 26}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', required=True, type=Path)
    parser.add_argument('--reference', required=True, type=Path)
    for name in ('module', 'abi-test', 'consumer', 'manifest', 'region', 'output', 'coexist'):
        parser.add_argument('--' + name, type=Path)
    args = parser.parse_args()
    if args.module:
        if not all(getattr(args, n) for n in ('abi_test', 'consumer', 'manifest', 'region', 'output')):
            parser.error('Validation needs module, ABI test, consumer, manifest, region and output')
        validate(args)
        return
    report = write_fixtures(args.fixtures.resolve(), args.reference.resolve())
    print(json.dumps({'positive': len(report['positive']), 'negative': len(report['negative']), 'all_reference_exact': True}))


if __name__ == '__main__':
    main()
