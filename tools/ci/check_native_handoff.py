"""Validate frozen native app/SDK installation without building the engine.

Development release check. Scientific interpretation/replay/export remains in
the installed C++ example, CLI and application. Removal is a separate checked
PowerShell step; this script never removes a directory or touches an old install.
"""
import argparse
import ast
import gzip
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import subprocess
import stat
import struct
import zipfile
import check_material_staging as admission


SUPPLEMENTAL = {
    'envi': 'envi-reader',
    'dicom': 'qupathxx-dicom-wsi',
}
DICOM_BIN = 'openslide-sdk/bin/'
DICOM_MODULE = 'qpx_dicom_wsi_plugin.dll'
DICOM_MANIFEST = 'dicom-wsi-reader-plugins.json'
OPENSLIDE_DLL = 'libopenslide-1.dll'
ENVI_MANIFEST = 'reader-plugins.json'
RAW_SAMPLES = (1, 257, 4096, 65535)


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


def files(root):
    return {p.relative_to(root).as_posix():sha(p) for p in sorted(root.rglob('*')) if p.is_file()}


def legacy_bundle(source):
    handoff=json.loads((source/'source-handoff.json').read_text(encoding='utf8'))
    frozen=source/'release-final/source'
    manifest=frozen/'tests/fixtures/manifest.json'
    if sha(manifest)!=handoff['canonical_inventory']['sha256']:
        raise ValueError('Frozen membership inventory changed')
    profile=json.loads(manifest.read_text(encoding='utf8'))['release_profiles']['windows-native']
    entries=admission.inventory([manifest,frozen/'assets/fonts/font-source.json',
        source/'release-final/generated-materials.json',source/'dependency-sources-final/generated-materials.json'])
    source_id=handoff['native_source_sha256']
    if profile['compiled_source_sha256']!=source_id:raise ValueError('Native source ID differs')
    archives={name:source/'release-final'/info['archive'] for name,info in handoff['archives'].items()}
    for name,path in archives.items():
        info=handoff['archives'][name]
        if sha(path)!=info['sha256'] or path.stat().st_size!=info['bytes']:
            raise ValueError('Frozen archive changed: '+name)
        admission.check_archive(path,entries)
    dependency=source/'dependency-sources-final'/handoff['dependency_companion']['archive']
    if sha(dependency)!=handoff['dependency_companion']['sha256']:
        raise ValueError('Dependency source companion changed')
    admission.check_archive(dependency,entries)
    return source_id, archives, profile['stages'], entries, set()


def check_archive(bundle, role):
    names = set()
    for member in bundle.infolist():
        raw = member.orig_filename
        logical = PurePosixPath(raw)
        kind = (member.external_attr >> 16) & 0o170000
        if (not logical.parts or logical.is_absolute() or raw != logical.as_posix()
                or any(c in '\\:<>"|?*' or ord(c) < 32 for c in raw) or '..' in logical.parts
                or any(p.casefold() in admission.RESTRICTED_PARTS for p in logical.parts)
                or any(p.rstrip(' .') != p or Path(p).is_reserved() for p in logical.parts)
                or raw.casefold() in names or member.is_dir() or kind not in (0, stat.S_IFREG)
                or member.flag_bits & 1):
            raise ValueError('Unsafe handoff archive member: ' + raw)
        if role == 'optional_providers' and not raw.startswith('providers/'):
            raise ValueError('Provider archive member outside providers directory')
        names.add(raw.casefold())
    return len(names)


def pinned_archive(source, role, name, record):
    path = source/name
    if not path.is_file() or path.stat().st_size != record['bytes'] or sha(path) != record['sha256']:
        raise ValueError('Handoff archive missing or differs from the pinned receipt: ' + role)
    return path


def published_bundle(source, expected_digest):
    # Pin the independently reviewed handoff receipt, then every archive it names.
    receipt = (source/'handoff.json').read_bytes()
    if hashlib.sha256(receipt).hexdigest() != expected_digest:
        raise ValueError('Handoff receipt differs from the pinned SHA-256')
    handoff = json.loads(receipt)
    source_id = handoff['source_sha256']
    records = handoff['archives']
    if (handoff['schema'] != 'qpx.windows-native-local-handoff/1' or handoff['state'] != 'verified_local'
            or handoff['python_runtime'] is not False or not re.fullmatch(r'[0-9a-f]{64}', source_id)
            or set(records) != {'native','sdk','source','dependency_source','optional_providers'}):
        raise ValueError('Unsupported Windows native handoff receipt')
    archives = {}
    for role, record in records.items():
        name = record['filename']
        if not re.fullmatch(r'qupathxx-[a-z0-9.-]+\.zip', name) or name in {path.name for path in archives.values()}:
            raise ValueError('Invalid or duplicate handoff archive name')
        path = pinned_archive(source, role, name, record)
        with zipfile.ZipFile(path) as bundle:
            if check_archive(bundle, role) != record['members']:
                raise ValueError('Handoff archive membership count differs: ' + role)
            if role in {'native','sdk','source'}:
                metadata = json.loads(bundle.read('RELEASE.json'))
                if metadata['stage'] != role or metadata['source_sha256'] != source_id:
                    raise ValueError('Handoff archive source identity differs: ' + role)
            if role == 'source' and hashlib.sha256(bundle.read('source-inputs.sha256')).hexdigest() != source_id:
                raise ValueError('Handoff source manifest differs')
            if role == 'dependency_source' and json.loads(bundle.read('sources.json'))['source_sha256'] != source_id:
                raise ValueError('Dependency source companion identity differs')
        archives[role] = path
    for name, record in handoff.get('supplemental_provider_archives', {}).items():
        roles = [role for role, prefix in SUPPLEMENTAL.items()
                 if re.fullmatch(prefix + r'-[0-9]+\.[0-9]+\.[0-9]+-windows-x64\.zip', name)]
        if (len(roles) != 1 or roles[0] in archives
                or name in {path.name for path in archives.values()}):
            raise ValueError('Invalid or duplicate supplemental provider archive: ' + name)
        role = roles[0]
        path = pinned_archive(source, role, name, record)
        with zipfile.ZipFile(path) as bundle:
            check_archive(bundle, role)
            members = set(bundle.namelist())
            if role == 'envi':
                required = {ENVI_MANIFEST, 'qpx_envi_plugin.dll', 'licenses/qupathxx/LICENSE'}
            else:
                required = {DICOM_BIN + item for item in (DICOM_MODULE, DICOM_MANIFEST, OPENSLIDE_DLL)}
                required.update({'licenses/qupathxx/LICENSE', 'licenses/libdicom/LICENSE', 'licenses/uthash/LICENSE',
                                 'openslide-sdk/licenses/OpenSlide/COPYING.LESSER'})
            if not required <= members:
                raise ValueError('Supplemental provider archive lacks install members: ' + role)
            if role == 'dicom':
                with zipfile.ZipFile(archives['native']) as runtime:
                    if bundle.read(DICOM_BIN + OPENSLIDE_DLL) != runtime.read(OPENSLIDE_DLL):
                        raise ValueError('DICOM OpenSlide dependency differs from the native runtime')
        archives[role] = path
    validation = handoff.get('validation', {})
    position_reexport = 'backed_position_export_sha256' in validation
    if position_reexport and ('envi' not in archives or not re.fullmatch(
            r'[0-9a-f]{64}', validation['backed_position_export_sha256'])):
        raise ValueError('Backed position replay requires a valid receipt digest and the ENVI archive')
    checks = {'position_reexport'} if position_reexport else set()
    for key in ('matrix_market_gzip', 'matrix_market_zip', 'spatial_channel', 'verified_spatial_imports'):
        enabled = validation.get(key, False)
        if type(enabled) is not bool or (enabled and not position_reexport):
            raise ValueError(f'{key} verification requires the backed spatial exchange')
        if enabled:
            checks.add(key)
    ome_tiff_bit = validation.get('ome_tiff_bit', False)
    if type(ome_tiff_bit) is not bool:
        raise ValueError('Invalid OME-TIFF bit verification flag')
    if ome_tiff_bit:
        checks.add('ome_tiff_bit')
    return source_id, archives, None, None, checks


def install_supplemental(archives, installed, native):
    manifests = {}
    for role in (role for role in SUPPLEMENTAL if role in archives):
        provider = installed/'providers'/role
        with zipfile.ZipFile(archives[role]) as bundle:
            for name in bundle.namelist():
                if name.startswith(('licenses/', 'openslide-sdk/licenses/')):
                    target = provider/name
                elif role == 'envi' and name in (ENVI_MANIFEST, 'qpx_envi_plugin.dll'):
                    target = provider/name
                elif role == 'dicom' and name in (DICOM_BIN + DICOM_MODULE, DICOM_BIN + DICOM_MANIFEST):
                    target = native/PurePosixPath(name).name
                else:
                    continue
                target.parent.mkdir(parents=True, exist_ok=True)
                with bundle.open(name) as stream, target.open('xb') as output:
                    shutil.copyfileobj(stream, output)
        manifests[role] = provider/ENVI_MANIFEST if role == 'envi' else native/DICOM_MANIFEST
    return manifests


def check_raw_samples(raw, expected, provider, *, dtype='<u2', shape=(2, 2, 1)):
    data = raw.read_bytes()
    if data[:8] != b'\x93NUMPY\x01\x00':
        raise ValueError('Unexpected raw result encoding: ' + provider)
    offset = 10 + int.from_bytes(data[8:10], 'little')
    header = ast.literal_eval(data[10:offset].decode('ascii'))
    if header != dict(descr=dtype, fortran_order=False, shape=shape) or data[offset:] != expected:
        raise ValueError('Installed provider raw samples differ: ' + provider)


def check_envi(run, native, root, manifest):
    root.mkdir()
    header = root/'spectra.hdr'
    header.write_text('''ENVI
samples = 2
lines = 2
bands = 1
header offset = 0
file type = ENVI Standard
data type = 12
interleave = bip
byte order = 0
''', encoding='ascii')
    samples = b''.join(value.to_bytes(2, 'little') for value in RAW_SAMPLES)
    header.with_suffix('.img').write_bytes(samples)
    project, raw = root/'spectra.qpx', root/'spectra.npy'
    plugin = ['--reader-plugins', manifest]
    created = json.loads(run('envi-project-create', [native/'qupathxx_format_import.exe', *plugin,
                         'project-create', header, project, '--reader', 'org.qupathxx.envi']).read_bytes())
    if created['reader'] != 'org.qupathxx.envi':
        raise ValueError('Saved ENVI project uses the wrong provider')
    before = sha(project)
    image_id = created['image_id']
    run('envi-project-raw', [native/'qupathxx_region.exe', *plugin, '--raw-project', project, image_id, raw])
    check_raw_samples(raw, samples, 'ENVI')
    run('envi-desktop-open', [native/'qupathxx_app.exe', *plugin, '--project', project,
                            '--image-id', image_id, '--layout', root/'preferences.ini', '--smoke'])
    if sha(project) != before:
        raise ValueError('ENVI replay changed the saved project')
    return {'project_sha256': before, 'raw_sha256': sha(raw)}


def check_dicom(run, native, root, manifest):
    fixture = Path(__file__).resolve().parent/'dicom-wsi-fixture'
    series = root/'series'
    series.mkdir(parents=True)
    primary, companion = series/'z_part1.dcm', series/'a_part2.dcm'
    for path in (primary, companion):
        shutil.copyfile(fixture/path.name, path)
    primary_sha = sha(primary)
    project = root/'slide.qpx'
    plugin = ['--reader-plugins', manifest]
    created = json.loads(run('dicom-project-create', [native/'qupathxx_format_import.exe', *plugin,
                         'project-create', primary, project, '--reader', 'org.qupathxx.dicom-wsi']).read_bytes())
    if created['reader'] != 'org.qupathxx.dicom-wsi' or (created['width'], created['height']) != (27, 16):
        raise ValueError('Saved DICOM project uses the wrong provider or dimensions')
    before = sha(project)
    image_id = created['image_id']
    crop = [native/'qupathxx_region.exe', *plugin, '--project-region', project, image_id, 13, 4, 0, 12, 6]
    output = root/'crop.pam'
    replay = json.loads(run('dicom-project-crop', [*crop, output, '--receipt']).read_bytes())
    expected = '0acd09b59e5c345d2965262bf4aa10205dde048f4653f07df15111d49e649df1'
    source = replay['reader_source']
    if (sha(output) != expected or replay['sha256'] != expected or source['scope'] != 'compound_files'
            or [Path(item['path']) for item in source['companions']] != [companion]):
        raise ValueError('DICOM replay differs in pixels or guarded companion identity')
    run('dicom-desktop-open', [native/'qupathxx_app.exe', *plugin, '--project', project,
                             '--image-id', image_id, '--layout', root/'preferences.ini', '--smoke'])
    shutil.copyfile(companion, root/'original-companion.dcm')
    shutil.copyfile(fixture/'changed-instance.dcm', companion)
    changed = root/'changed.pam'
    run('dicom-changed-direct-crop', [native/'qupathxx_region.exe', *plugin, '--reader',
                                    'org.qupathxx.dicom-wsi', primary, 13, 4, 0, 12, 6, changed])
    changed_expected = '494308305235a2a2bf9ee478db2a609f9e2bd80b046bca01b5f437c54ef56a33'
    if sha(changed) != changed_expected:
        raise ValueError('Replacement companion does not yield the expected direct crop')
    refused = root/'refused.pam'
    diagnostic = run('dicom-changed-companion', [*crop, refused, '--receipt'], expected_exit=1).read_bytes()
    if refused.exists() or b'Reader plugin source files changed; review explicitly' not in diagnostic:
        raise ValueError('DICOM saved replay did not refuse the changed companion')
    if sha(project) != before or sha(primary) != primary_sha:
        raise ValueError('DICOM replay changed the saved project or primary instance')
    return {'project_sha256': before, 'crop_sha256': expected, 'changed_crop_sha256': changed_expected,
            'changed_companion_refused': True}


def check_position_reexport(run, native, user, manifest, checks):
    root = user/'spatial exchange'
    root.mkdir()
    matrix, features, barcodes = (root/name for name in ('matrix.mtx', 'features.tsv', 'barcodes.tsv'))
    market = (b'%%MatrixMarket matrix coordinate integer general\n2 3 3\n'
              b'1 1 18446744073709551615\n1 3 0\n2 2 9007199254740993\n')
    matrix.write_bytes(market)
    features.write_bytes(b'A\tGene A\tGene Expression\nB\tGene B\tGene Expression\n')
    barcodes.write_bytes('0001\n1\nμ\n'.encode('utf8'))
    indexed, positions = root/'exchange.qmx', root/'positions.tsv'
    header = 'observation_id\tx_px\ty_px\n'
    rows = ['0001\t0.5\t0.75\n', '1\t1.25\t1.25\n', 'μ\t0.25\t1.5\n']
    positions.write_bytes((header + rows[2] + rows[0] + rows[1]).encode('utf8'))
    expected = (header + ''.join(rows)).encode('utf8')
    cli = native/'qupathxx_format_import.exe'
    inputs = [matrix, features, barcodes]
    matrix_market_gzip = 'matrix_market_gzip' in checks
    if matrix_market_gzip:
        inputs = [path.with_suffix(path.suffix + '.gz') for path in inputs]
        for source, dest in zip((matrix, features, barcodes), inputs):
            dest.write_bytes(gzip.compress(source.read_bytes(), mtime=0))
        bad = root/'bad-crc.tsv.gz'
        corrupt = bytearray(inputs[1].read_bytes())
        corrupt[-8] ^= 1
        bad.write_bytes(corrupt)
        rejected = root/'refused.qmx'
        log = run('spatial-reject-gzip-crc', [cli, 'matrix-market-index', inputs[0], bad, inputs[2], rejected,
                  '--catalogue', 'tenx'], expected_exit=2)
        if rejected.exists() or 'Invalid or truncated Matrix Market gzip' not in json.loads(log.read_bytes())['error']:
            raise ValueError('Spatial index did not refuse the corrupted gzip catalogue')
    index = json.loads(run('spatial-index', [cli, 'matrix-market-index', *inputs,
                            indexed, '--catalogue', 'tenx']).read_bytes())
    plugin = [cli, '--reader-plugins', manifest]
    base = user/'envi/spectra.qpx'
    base_sha = sha(base)
    project = root/'restored.qpx'
    explicit_channel = 'spatial_channel' in checks
    plane_options = ['--frame-channel', 0] if explicit_channel else []
    restore = json.loads(run('spatial-restore', [*plugin, 'project-matrix-import-positions', base, 1,
                             indexed, positions, project, *plane_options]).read_bytes())
    if not restore['owned'] or restore['observations'] != len(rows):
        raise ValueError('Spatial restore did not retain the whole matrix and point catalogue')
    if explicit_channel and restore['plane'] != dict(channel=0, z=0, time=0):
        raise ValueError('Spatial restore did not retain the explicit channel')
    before = sha(project)
    offline = root/'offline inputs'
    offline.mkdir()
    for path in (indexed, positions):
        path.rename(offline/path.name)
    association = restore['matrix_source_sha256']
    common = [project, restore['image_id'], association]
    replay = root/'positions-replayed.tsv'
    export = [*plugin, 'project-matrix-export-positions', *common]
    report = json.loads(run('spatial-reexport-positions', [*export, replay, '--frame-id',
                            restore['coordinate_frame'], *plane_options]).read_bytes())
    if (replay.read_bytes() != expected or report['coordinate_frame'] != restore['coordinate_frame']
            or report['matrix_file_sha256'] != index['file_sha256']):
        raise ValueError('Spatial export differs in observation order, positions or saved identity')
    rejected = root/'refused.tsv'
    log = run('spatial-reject-wrong-frame', [*export, rejected, '--frame-id', 'unrelated', *plane_options], expected_exit=2)
    if rejected.exists() or 'No backed or indexed expression observations' not in json.loads(log.read_bytes())['error']:
        raise ValueError('Spatial export did not refuse the wrong saved frame')
    counts = root/'counts.zip'
    run('spatial-reexport-counts', [*plugin, 'project-matrix-export-mtx', *common, counts])
    with zipfile.ZipFile(counts) as bundle:
        if (bundle.read('matrix.mtx') != market or bundle.read('features.tsv') != features.read_bytes()
                or bundle.read('barcodes.tsv') != barcodes.read_bytes()):
            raise ValueError('Restored counts, sparse zeros or catalogues differ')
    direct_qmx_sha256 = None
    if 'matrix_market_zip' in checks:
        report_bytes = run('spatial-format-catalog', [cli, '--formats']).read_bytes()
        for frontend in ('region', 'batch'):
            if run(frontend+'-format-catalog', [native/f'qupathxx_{frontend}.exe', '--formats']).read_bytes() != report_bytes:
                raise ValueError('Installed frontends disagree on format capabilities')
        catalog = json.loads(report_bytes)
        capability, = [row for row in catalog['matrix'] if row['provider_id'] == 'matrix-market']
        if capability['can_import_zip'] is not True or capability['can_import_gzip'] is not matrix_market_gzip:
            raise ValueError('Installed Matrix Market capabilities differ from the handoff')
        reference = index
        if matrix_market_gzip:
            reference = json.loads(run('spatial-index-unpacked', [cli, 'matrix-market-index', matrix, features,
                                       barcodes, root/'unpacked.qmx', '--catalogue', 'tenx']).read_bytes())
        direct = root/'direct.qmx'
        receipt = json.loads(run('spatial-index-zip', [cli, 'matrix-market-index-zip', counts, direct]).read_bytes())
        direct_qmx_sha256 = sha(direct)
        if (direct_qmx_sha256 != reference['file_sha256'] or direct_qmx_sha256 != receipt['file_sha256']
                or receipt['source_sha256'] != reference['source_sha256']):
            raise ValueError('Direct ZIP indexing differs from the exact unpacked QMX')
    if sha(project) != before or sha(base) != base_sha:
        raise ValueError('Spatial exchange changed a saved input project')
    return {'project_sha256': before, 'positions_sha256': sha(replay), 'counts_sha256': sha(counts),
            'matrix_market_gzip': matrix_market_gzip, 'direct_qmx_sha256': direct_qmx_sha256, 'explicit_channel': explicit_channel}


def check_verified_spatial_source(run, native, root):
    root.mkdir()
    image = root/'target.ppm'
    ppm_header = b'P6\n2 2\n255\n'
    image.write_bytes(ppm_header + bytes(range(12)))
    cli = native/'qupathxx_format_import.exe'
    base, verified, adopted = (root/name for name in ('base.qpx', 'verified.qpx', 'adopted.qpx'))
    run('verified-spatial-create', [cli, 'project-create', image, base, '--reader', 'ppm'])
    first = json.loads(run('verified-spatial-review', [cli, 'project-review-source', base, 1, image]).read_bytes())
    run('verified-spatial-bind', [cli, 'project-adopt-source', base, 1, image, first['review_sha256'], verified])
    before = sha(verified)

    matrix, features, barcodes, positions, indexed = (root/name for name in
        ('matrix.mtx', 'features.tsv', 'barcodes.tsv', 'positions.tsv', 'source.qmx'))
    matrix.write_bytes(b'%%MatrixMarket matrix coordinate integer general\n1 1 1\n1 1 7\n')
    features.write_bytes(b'feature_id\nG\n')
    barcodes.write_bytes(b'observation_id\nA\n')
    positions.write_bytes(b'observation_id\tx_px\ty_px\nA\t1\t1\n')
    run('verified-spatial-index', [cli, 'matrix-market-index', matrix, features, barcodes,
                                  indexed, '--catalogue', 'qpx'])
    replacement = ppm_header + bytes(range(1, 13))
    image.write_bytes(replacement)
    backed, selected = root/'backed.qpx', root/'selected.qpx'

    def backed_import(project):
        return [cli, 'project-matrix-import-positions', project, 1, indexed, positions, backed]

    def selected_import(project):
        return [native/'qupathxx_spatial_counts.exe', project, selected, 1, matrix, features, positions, '--gene', 'G']

    for route, command, output in [('backed', backed_import, backed), ('selected', selected_import, selected)]:
        log = run('verified-spatial-refuse-'+route, command(verified), expected_exit=2)
        if output.exists() or b'differs from saved source verification' not in log.read_bytes():
            raise ValueError('Spatial import accepted a changed verified native image: ' + route)
    review = json.loads(run('verified-spatial-replacement', [cli, 'project-review-source', verified, 1, image]).read_bytes())
    if review['comparison'] != 'changed_bytes' or not review['requires_acceptance']:
        raise ValueError('Native source review did not require acceptance of the replacement')
    run('verified-spatial-adopt', [cli, 'project-adopt-source', verified, 1, image, review['review_sha256'],
                                   adopted, '--accept-changed'])
    for route, command in [('backed', backed_import), ('selected', selected_import)]:
        result = json.loads(run('verified-spatial-import-'+route, command(adopted)).read_bytes())
        if result['observations'] != 1:
            raise ValueError('Adopted native image did not publish the expected observation: ' + route)
    if sha(verified) != before or image.read_bytes() != replacement:
        raise ValueError('Verified source checks changed an input project or image')
    return {'verified_project_sha256': before, 'backed_sha256': sha(backed), 'selected_sha256': sha(selected)}


def check_ome_tiff_bit(run, native, root):
    root.mkdir()
    source = root/'oriented.ome.tif'
    xml = (b'<OME xmlns="http://www.openmicroscopy.org/Schemas/OME/2016-06"><Image ID="Image:0">'
           b'<Pixels ID="Pixels:0" SizeX="3" SizeY="2" SizeZ="1" SizeC="1" SizeT="1" '
           b'DimensionOrder="XYZCT" Type="bit" SignificantBits="1" PhysicalSizeX="2" PhysicalSizeY="4">'
           b'<Channel ID="Channel:0:0" SamplesPerPixel="1"/><TiffData/></Pixels></Image></OME>\0')
    # Stored 3x2 MSB-first rows [0,1,1] and [1,0,0]. TIFF orientation 6
    # rotates them clockwise into a 2x3 image. MINISWHITE defines display polarity.
    tags = [(256, 4, 1, 3), (257, 4, 1, 2), (258, 3, 1, 1), (259, 3, 1, 1), (262, 3, 1, 0),
            (266, 3, 1, 1), (270, 2, len(xml), 0), (273, 4, 1, 0), (274, 3, 1, 6),
            (277, 3, 1, 1), (278, 4, 1, 2), (279, 4, 1, 2), (284, 3, 1, 1)]
    xml_offset = 8 + 2 + 12 * len(tags) + 4
    offsets = {270: xml_offset, 273: xml_offset + len(xml)}
    directory = b''.join(struct.pack('<HHII', tag, kind, count, offsets.get(tag, value))
                         for tag, kind, count, value in tags)
    fixture = struct.pack('<2sHIH', b'II', 42, 8, len(tags)) + directory + bytes(4) + xml + b'\x60\x80'
    source.write_bytes(fixture)
    region = native/'qupathxx_region.exe'
    bundle, raw = root/'exact.zip', root/'exact.npy'
    run('ome-bit-bundle', [region, '--raw', source, bundle, '--bundle'])
    result = json.loads(run('ome-bit-extract', [region, '--extract-raw-bundle', bundle, raw]).read_bytes())
    calibration = result['calibration']
    if (calibration['base_pixel_width_um'], calibration['base_pixel_height_um']) != (4, 2):
        raise ValueError('OME-TIFF orientation did not preserve physical pitch')
    samples = bytes([1, 0, 0, 1, 0, 1])
    check_raw_samples(raw, samples, 'OME-TIFF bit', dtype='|u1', shape=(3, 2, 1))
    project = root/'oriented.qpx'
    info = json.loads(run('ome-bit-create-project', [native/'qupathxx_format_import.exe', 'project-create',
                         source, project, '--reader', 'tiff']).read_bytes())
    if (info['width'], info['height']) != (2, 3):
        raise ValueError('Saved OME-TIFF frame does not have oriented dimensions')
    frozen = sha(project)
    replay = root/'replay.npy'
    run('ome-bit-project-raw', [region, '--raw-project', project, info['image_id'], replay])
    if replay.read_bytes() != raw.read_bytes():
        raise ValueError('Saved OME-TIFF project raw replay differs')
    display = root/'display.pam'
    run('ome-bit-project-display', [region, '--project-region', project, info['image_id'], 0, 0, 0, 2, 3, display])
    expected_display = b''.join(bytes([0 if bit else 255])*3 + b'\xff' for bit in samples)
    if display.read_bytes().split(b'ENDHDR\n', 1)[-1] != expected_display or sha(project) != frozen:
        raise ValueError('Saved OME-TIFF display polarity or project bytes differ')
    malformed, refused = root/'contradictory.ome.tif', root/'refused.npy'
    malformed.write_bytes(fixture.replace(b'SignificantBits="1"', b'SignificantBits="2"'))
    log = run('ome-bit-refuse-metadata', [region, '--raw', malformed, refused], expected_exit=2)
    if refused.exists() or b'SignificantBits of one' not in log.read_bytes() or source.read_bytes() != fixture:
        raise ValueError('OME-TIFF contradictory metadata did not refuse cleanly')
    return {'project_sha256': frozen, 'raw_sha256': sha(raw), 'display_sha256': sha(display)}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True,help='Frozen Windows handoff directory')
    parser.add_argument('--handoff-sha256',help='Pinned SHA-256 of handoff.json for a native archive handoff')
    parser.add_argument('--work',type=Path,required=True,help='New, short external validation directory')
    parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--cmake',type=Path,help='Location of cmake.exe if absent from PATH')
    args=parser.parse_args()
    if os.name!='nt':parser.error('This handoff is Windows x64 only')
    source,work,report=args.input.resolve(),args.work.absolute(),args.report.resolve()
    if args.handoff_sha256:
        source_id, archives, stages, entries, checks = published_bundle(source, args.handoff_sha256.lower())
    else:
        source_id, archives, stages, entries, checks = legacy_bundle(source)
    report.mkdir(parents=True,exist_ok=True)
    # New prefix only, outside the checkout/cache and the old rejected TEMP tree.
    work.mkdir(parents=True,exist_ok=False)
    for part in (work,*work.parents):
        if admission.reparse(part):raise ValueError('Validation prefix contains a link/reparse point')
    native,sdk=work/'installed/native app',work/'installed/native sdk'
    for name,target in [('native',native),('sdk',sdk)]:
        target.mkdir(parents=True)
        with zipfile.ZipFile(archives[name]) as archive:archive.extractall(target)
        if stages is not None:
            admission.check_stage(target,entries)
            for member in stages[name]:
                record=entries[member['material_id']]
                if sha(target/member['path'])!=record['sha256']:
                    raise ValueError('Installed path identity changed: '+member['path'])
    if args.handoff_sha256:
        with zipfile.ZipFile(archives['optional_providers']) as archive:
            archive.extractall(work/'installed')
    manifests = install_supplemental(archives, work/'installed', native)
    installed={str(p):sha(p) for p in (work/'installed').rglob('*') if p.is_file()}
    consumer,build=work/'example',work/'b'
    shutil.copytree(sdk/'share/qupathxx/examples/native_sdk',consumer)
    env=os.environ.copy()
    for key in list(env):
        if key.upper() in {'PYTHONPATH','PYTHONHOME','CMAKE_PREFIX_PATH','QUPATHXX_DIR','QUPATHXX_UI_LANGUAGE'}:
            env.pop(key)
    windows=Path(os.environ['SystemRoot'])
    env['PATH']=os.pathsep.join([str(windows/'System32'),str(windows)])
    cmake=args.cmake or shutil.which('cmake.exe') or Path('C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')
    commands=[]

    def run(name,command,cwd=work,expected_exit=0):
        command=[str(x) for x in command]
        commands.append({'name':name,'argv':command,'cwd':str(cwd),'expected_exit':expected_exit})
        with (report/(name+'.log')).open('wb') as log:
            result=subprocess.run(command,cwd=cwd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
        commands[-1]['exit_code'] = result.returncode
        (report/'commands.json').write_text(json.dumps(commands,indent=2)+'\n',encoding='utf8')
        if result.returncode != expected_exit:
            raise RuntimeError(name+' failed; retained '+str(report/(name+'.log')))
        print(name+': passed',flush=True)
        return report/(name+'.log')

    run('sdk-configure',[cmake,'-S',consumer,'-B',build,'-G','Visual Studio 17 2022','-A','x64',
        '-Dqupathxx_DIR='+str(sdk/'lib/cmake/qupathxx'),'-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF',
        '-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF'])
    run('sdk-build',[cmake,'--build',build,'--config','Release','--parallel','2','--','/nr:false'])
    buildlog=(report/'sdk-build.log').read_text(encoding='utf8',errors='replace')
    include_paths=[Path(x.strip()).resolve() for x in re.findall(r'including file:\s*([^\r\n]+)',buildlog)]
    cache=(build/'CMakeCache.txt').read_text(encoding='utf8')
    vs=re.search(r'^CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)$',cache,re.MULTILINE)
    if not vs or not vs[1].strip():raise ValueError('Missing Visual Studio generator instance')
    allowed=[sdk.resolve(),Path(vs[1].strip()).resolve(),Path('C:/Program Files (x86)/Windows Kits'),
             Path('C:/Program Files/Windows Kits')]
    if not include_paths or any(not any(p.is_relative_to(a) for a in allowed) for p in include_paths):
        raise ValueError('Consumer includes escape installed SDK/Windows toolchain')
    if 'LNK4098' in buildlog:raise ValueError('Consumer CRT link conflict')
    for forbidden in ['C:/Users/HEQ/Projects/qupathxx/build/ci','C:/Users/HEQ/Projects/qupathxx/build/deps']:
        if forbidden.casefold() in buildlog.replace('\\','/').casefold():
            raise ValueError('Consumer refers to original source/dependency cache')
    user=work/'user data';user.mkdir()
    results=user/'native results'
    run('sdk-use',[build/'Release/sdk_consumer.exe',results],cwd=windows/'System32')
    result=json.loads((results/'consumer.json').read_text())
    if result['source_sha256']!=source_id or not result['saved_replay']:
        raise ValueError('Installed header/library or saved identity differs')
    project=results/'saved.qpx';before_project=sha(project)
    batch=native/'qupathxx_batch.exe'
    common=['--image','1','--record',result['de_record']]
    run('cli-verify-de',[batch,'--verify-expression-differential',project,*common,'--work-bytes','67108864'])
    run('cli-export-de',[batch,'--export-expression-differential',project,user/'genes.csv',*common])
    run('cli-export-replicates',[batch,'--export-expression-differential',project,user/'replicates.csv',*common,'--replicates'])
    for name in ['genes.csv','replicates.csv']:
        if (user/name).read_bytes()!=(results/name).read_bytes():raise ValueError('CLI/SDK export differs: '+name)
    common=['--image','1','--record',result['pca_id'],'--work-bytes','67108864']
    run('cli-replay-pca',[batch,'--replay-expression-reduction',project,*common])
    run('cli-plot-pca',[batch,'--export-expression-reduction',project,user/'embedding.svg',*common,'--plot'])
    # Authored RGB fixture and independent expected crop; no external images.
    pixels=bytes(range(36));image=user/'authored.ppm'
    image.write_bytes(b'P6\n4 3\n255\n'+pixels)
    run('region-crop',[native/'qupathxx_region.exe',image,'1','1','0','2','1',user/'crop.pam'])
    output=(user/'crop.pam').read_bytes().split(b'ENDHDR\n',1)
    expected=b''.join(pixels[(4+x)*3:(4+x+1)*3]+b'\xff' for x in [1,2])
    if len(output)!=2 or output[1]!=expected:raise ValueError('Native region samples differ')
    if args.handoff_sha256:
        image=user/'samples.vti'
        image.write_text('''<VTKFile type="ImageData" version="1.0" byte_order="LittleEndian">
<ImageData WholeExtent="0 1 0 1 0 0" Origin="0 0 0" Spacing="1 1 1">
<Piece Extent="0 1 0 1 0 0"><PointData Scalars="Samples">
<DataArray type="UInt16" Name="Samples" format="ascii">1 257 4096 65535</DataArray>
</PointData></Piece></ImageData></VTKFile>''',encoding='utf8')
        raw=user/'samples.npy'
        manifest=work/'installed/providers/vti/reader-plugins.json'
        run('vti-raw',[native/'qupathxx_region.exe','--reader-plugins',manifest,'--reader','org.qupathxx.vti',
                       '--reader-options','{"point_array":"Samples"}','--raw',image,raw])
        check_raw_samples(raw, b''.join(v.to_bytes(2, 'little') for v in RAW_SAMPLES), 'VTI')
    supplemental = {}
    for role, check in [('envi', check_envi), ('dicom', check_dicom)]:
        if role in manifests:
            supplemental[role] = check(run, native, user/role, manifests[role])
    spatial = check_position_reexport(run, native, user, manifests['envi'], checks) if 'position_reexport' in checks else None
    verified_spatial = check_verified_spatial_source(run, native, user/'verified spatial source') if 'verified_spatial_imports' in checks else None
    ome_bit = check_ome_tiff_bit(run, native, user/'ome raster') if 'ome_tiff_bit' in checks else None
    run('gui-project-open',[native/'qupathxx_app.exe','--project',project,'--layout',user/'preferences.ini','--smoke'])
    if sha(project)!=before_project:raise ValueError('Read-only GUI/CLI changed saved project')
    sentinel=native/'user scratch/research-notes.txt';sentinel.parent.mkdir()
    sentinel.write_text('Authored research notes must survive package removal.\n',encoding='utf8')
    protected={str(p):sha(p) for p in user.rglob('*') if p.is_file()}
    protected[str(sentinel)]=sha(sentinel)
    if any(sha(Path(p))!=digest for p,digest in installed.items()):raise ValueError('Installed package changed during use')
    receipt={'state':'install_and_use_passed','scope':'local Windows fresh per-user prefix; native only; no engine build',
        'source_sha256':source_id,'os':platform.platform(),'work':str(work),'native':str(native),'sdk':str(sdk),
        'installed_files':installed,'protected_user_files':protected,'consumer':result,
        'compiler_include_records':len(include_paths),'runtime_PATH':env['PATH'],
        'python_paths':'removed','engine_builds':0,'install_run_verified':True,'uninstall_verified':False}
    if args.handoff_sha256:
        receipt['archive_handoff_sha256']=args.handoff_sha256.lower()
    if supplemental:
        receipt['supplemental_provider_replay'] = supplemental
    if spatial:
        receipt['spatial_position_reexport'] = spatial
    if verified_spatial:
        receipt['verified_spatial_imports'] = verified_spatial
    if ome_bit:
        receipt['ome_tiff_bit'] = ome_bit
    (report/'handoff.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8')
    print('Install/use passed; manifest-owned removal is still required.',flush=True)


if __name__=='__main__':main()
