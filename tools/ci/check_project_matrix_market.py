"""Check native saved-count export against the exact stored sparse rows."""

import argparse
from contextlib import closing
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import sqlite3
import struct
import subprocess
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile, ZipInfo


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def stored_rows(path):
    connection = sqlite3.connect(path.as_uri() + "?mode=ro&immutable=1", uri=True)
    try:
        features = connection.execute("SELECT idx,id,name,type FROM features ORDER BY idx").fetchall()
        barcodes = connection.execute("SELECT idx,id FROM observations ORDER BY idx").fetchall()
        counts = [(feature + 1, observation + 1, int.from_bytes(value, "little"))
                  for feature, observation, value in connection.execute(
                      "SELECT feature,observation,value FROM counts ORDER BY feature,observation")]
    finally:
        connection.close()
    assert [idx for idx, *_ in features] == list(range(len(features)))
    assert [idx for idx, _ in barcodes] == list(range(len(barcodes)))
    return [row[1:] for row in features], [identifier for _, identifier in barcodes], counts


def check_real_exchange(run, output, project, image_id, item):
    source = Path(item['path'])
    original = (sha(project), sha(source))
    archive = output/'continuous-export.zip'
    report = run('real-export', 'project-matrix-export-mtx', project, image_id,
                 item['association_id'], archive, '--real')
    real_dir = output/'real'
    real_dir.mkdir()
    names = ('matrix.mtx', 'features.tsv', 'barcodes.tsv')
    with ZipFile(archive) as bundle:
        assert bundle.namelist() == list(names) and bundle.testzip() is None
        for name in names:
            (real_dir/name).write_bytes(bundle.read(name))
    matrix, features, barcodes = [real_dir/name for name in names]
    assert matrix.read_bytes().startswith(b'%%MatrixMarket matrix coordinate real general\n')
    indexed, plain = real_dir/'indexed.qmx', real_dir/'plain.qmx'
    imported = run('real-index-zip', 'matrix-market-index-zip', archive, indexed, '--real')
    run('real-index-files', 'matrix-market-index', matrix, features, barcodes, plain, '--catalogue', 'tenx', '--real')
    assert imported['domain'] == 'continuous' and imported['value_domain'] == report['value_domain']
    assert sha(indexed) == sha(plain) == imported['file_sha256'] and stored_rows(indexed) == stored_rows(source)

    sample = real_dir/'edge.mtx'
    sample.write_bytes(b'%%MatrixMarket matrix coordinate real general\n3 2 5\n'
                       b'1 1 -0\n1 2 0\n2 1 -1.25\n2 2 +5e-324\n3 1 1.7976931348623157e308\n')
    features_qpx, observations_qpx = real_dir/'features-qpx.tsv', real_dir/'observations-qpx.tsv'
    features_qpx.write_bytes(b'feature_id\nA\nB\nC\n')
    observations_qpx.write_bytes(b'observation_id\n0001\n1\n')
    edges = real_dir/'edges.qmx'

    def index_real(label, source_path, target, status=0):
        return run(label, 'matrix-market-index', source_path, features_qpx, observations_qpx,
                   target, '--catalogue', 'qpx', '--real', status=status)

    report = index_real('real-edge-values', sample, edges)
    assert report['value_domain'] == 'matrix-market-real'
    with closing(sqlite3.connect(edges.as_uri() + '?mode=ro&immutable=1', uri=True)) as database:
        assert database.execute('PRAGMA user_version').fetchone() == (7,)
        assert database.execute("SELECT name FROM sqlite_schema WHERE name='coordinates'").fetchone() is None
    _, ids, entries = stored_rows(edges)
    assert ids == ['0001', '1']
    values = [-0.0, 0.0, -1.25, 5e-324, float.fromhex('0x1.fffffffffffffp+1023')]
    assert [entry[2] for entry in entries] == [int.from_bytes(struct.pack('<d', v),'little') for v in values]
    bad, target = real_dir/'bad.mtx', real_dir/'refused.qmx'
    for label, row in [('nonfinite', '1 1 nan'), ('overflow', '1 1 1e999'), ('duplicate', '1 1 0\n1 1 -1')]:
        bad.write_text('%%MatrixMarket matrix coordinate real general\n3 2 '+
                       str(row.count('\n')+1)+'\n'+row+'\n', encoding='ascii')
        index_real('real-refuse-'+label, bad, target, status=2)
        assert not target.exists()
    run('real-refuse-count-mode', 'matrix-market-index-zip', archive, target, status=2)
    assert not target.exists()

    labels = stored_rows(indexed)[1]
    positions = real_dir/'positions.tsv'
    positions.write_text('observation_id\tx_px\ty_px\n'+''.join(f'{label}\t{i}\t1\n' for i,label in enumerate(labels)),
                         encoding='utf-8')
    image, base, saved = real_dir/'source.ppm', real_dir/'base.qpx', real_dir/'restored.qpx'
    width = max(len(labels), 2)
    image.write_bytes(f'P6\n{width} 2\n255\n'.encode('ascii')+bytes([80])*(width*6))
    run('real-project', 'project-create', image, base, '--reader', 'ppm')
    restored = run('real-restore', 'project-matrix-import-positions', base, '1', indexed, positions, saved)
    indexed.rename(real_dir/'indexed-offline.qmx')
    replay = real_dir/'replayed.zip'
    run('real-reexport', 'project-matrix-export-mtx', saved, '1', restored['matrix_source_sha256'], replay, '--real')
    assert sha(replay) == sha(archive) and (sha(project), sha(source)) == original


def member_lines(archive, name):
    payload = archive.read(name).decode("utf-8")
    assert payload.endswith("\n"), name
    return payload[:-1].split("\n")


def inspect_archive(path, expected):
    features, barcodes, counts = expected
    with ZipFile(path) as archive:
        assert sorted(archive.namelist()) == ["barcodes.tsv", "features.tsv", "matrix.mtx"]
        assert archive.testzip() is None
        assert all(member.compress_type == ZIP_STORED for member in archive.infolist())
        assert [tuple(line.split("\t")) for line in member_lines(archive, "features.tsv")] == features
        assert member_lines(archive, "barcodes.tsv") == barcodes
        header, *rows = member_lines(archive, "matrix.mtx")
        assert header == "%%MatrixMarket matrix coordinate integer general"
        dimensions, *rows = [line for line in rows if not line.startswith("%")]
        assert tuple(map(int, dimensions.split())) == (len(features), len(barcodes), len(counts))
        assert [tuple(map(int, line.split())) for line in rows] == counts


def check_archive_member_order(run, output, archive, inputs, exact):
    reordered = output/'reordered.zip'
    names = ('barcodes.tsv', 'matrix.mtx', 'features.tsv')
    with ZipFile(reordered, 'x', ZIP_DEFLATED) as zipped:
        for name in names:
            zipped.writestr(name, inputs[name])
    data = reordered.read_bytes()
    directory = int.from_bytes(data[-6:-2], 'little')
    offset = directory
    records = []
    for name in names:
        # The test producer emits no extra fields or comments.
        size = 46 + len(name)
        records.append(data[offset:offset+size])
        offset += size
    assert offset == len(data) - 22
    reordered.write_bytes(data[:directory] + b''.join(reversed(records)) + data[offset:])
    with ZipFile(reordered) as zipped:
        assert zipped.namelist() == list(reversed(names)) and zipped.testzip() is None
    reordered_hash = sha(reordered)
    result = output/'reordered.qmx'
    restored = run('archive-reordered', 'matrix-market-index-zip', reordered, result)
    assert sha(result) == sha(exact) == restored['file_sha256']
    central_name = directory + 46
    for name, payload, diagnostic in [
        ('duplicate-name', data[:central_name] + b'features.tsv' + data[central_name+12:], 'duplicate ZIP member name'),
        ('unknown-name', data[:central_name] + b'barcodes.txt' + data[central_name+12:], 'unexpected ZIP member name'),
        ('duplicate-offset', data[:directory+len(records[0])+42] + bytes(4)
         + data[directory+len(records[0])+46:], 'central directory repeats a ZIP member offset'),
        ('unknown-offset', data[:directory+42] + (1).to_bytes(4, 'little')
         + data[directory+46:], 'central directory references an unknown member offset'),
    ]:
        invalid, refused = output/(name+'.zip'), output/(name+'.qmx')
        invalid.write_bytes(payload)
        status = run('archive-'+name, 'matrix-market-index-zip', invalid, refused, status=2)
        assert diagnostic in status['error'] and not refused.exists()
    assert sha(reordered) == reordered_hash and sha(archive) != reordered_hash
    return result


def check_streaming_archive(run, output, inputs, reference):
    class Stream(io.BytesIO):
        def seek(self, *args):
            raise OSError('test archive stream cannot seek')

    stream = Stream()
    timestamp = struct.pack('<HHBI', 0x5455, 5, 1, 0)
    with ZipFile(stream, 'w') as zipped:
        for name in ('barcodes.tsv', 'features.tsv', 'matrix.mtx'):
            entry = ZipInfo(name)
            entry.extra = timestamp
            entry.compress_type = ZIP_STORED if name == 'features.tsv' else ZIP_DEFLATED
            zipped.writestr(entry, inputs[name])
    data = stream.getvalue()
    directory = int.from_bytes(data[-6:-2], 'little')
    with ZipFile(io.BytesIO(data)) as archive:
        assert archive.testzip() is None and all(member.flag_bits & 8 for member in archive.infolist())
    signed_offset = directory - 16
    assert data[signed_offset:signed_offset+4] == b'PK\x07\x08'
    unsigned = bytearray(data[:signed_offset] + data[signed_offset+4:])
    unsigned[-6:-2] = (directory-4).to_bytes(4, 'little')
    for label, payload in [('signed', data), ('unsigned', unsigned)]:
        archive, result = output/(label+'-descriptor.zip'), output/(label+'-descriptor.qmx')
        archive.write_bytes(payload)
        report = run('archive-descriptor-'+label, 'matrix-market-index-zip', archive, result)
        assert sha(result) == sha(reference) == report['file_sha256']
        assert archive.read_bytes() == payload
    for label, offset, diagnostic in [
        ('crc', signed_offset+4, 'ZIP data descriptor differs'),
        ('signature', signed_offset, 'invalid ZIP data descriptor signature'),
        ('placeholder', 18, 'central directory differs from ZIP member'),
        ('unknown-extra', 30+len('barcodes.tsv'), 'unsupported or repeated ZIP extra field'),
    ]:
        damaged = bytearray(data)
        damaged[offset] ^= 1
        archive, target = output/('descriptor-bad-'+label+'.zip'), output/('descriptor-bad-'+label+'.qmx')
        archive.write_bytes(damaged)
        refused = run('archive-descriptor-bad-'+label, 'matrix-market-index-zip', archive, target, status=2)
        assert diagnostic in refused['error'] and not target.exists()
    return result


def check_deflated_archive(run, output, expected, descriptors):
    names = ('matrix.mtx', 'features.tsv', 'barcodes.tsv')
    inputs = {name: (output/name).read_bytes() for name in names}
    # Exercise multiple compressed and expanded chunks with legal comments.
    title, rows = inputs['matrix.mtx'].split(b'\n', 1)
    comments = b''.join(b'% ' + hashlib.sha256(str(i).encode('ascii')).hexdigest().encode('ascii') + b'\n'
                        for i in range(4096))
    inputs['matrix.mtx'] = title + b'\n' + comments + rows
    reference_matrix = output/'deflate-reference.mtx'
    reference_matrix.write_bytes(inputs['matrix.mtx'])
    reference = output/'deflate-reference.qmx'
    plain = run('deflate-reference', 'matrix-market-index', reference_matrix, 'features.tsv',
                'barcodes.tsv', reference, '--catalogue', 'tenx')
    stored_archive, stored = output/'stored-reference.zip', output/'stored-reference.qmx'
    with ZipFile(stored_archive, 'x') as zipped:
        for name in names:
            zipped.writestr(name, inputs[name])
    run('stored-reference', 'matrix-market-index-zip', stored_archive, stored)
    archive = output/'recompressed.zip'
    with ZipFile(archive, 'x', ZIP_DEFLATED) as zipped:
        for name in names:
            zipped.writestr(name, inputs[name], compress_type=ZIP_STORED if name == 'barcodes.tsv' else ZIP_DEFLATED)
    with ZipFile(archive) as zipped:
        assert zipped.getinfo('matrix.mtx').compress_size > 65536
        assert zipped.testzip() is None
    archive_hash = sha(archive)
    imported = output/'recompressed.qmx'
    direct = run('archive-deflate', 'matrix-market-index-zip', archive, imported)
    assert plain['source_sha256'] == direct['source_sha256']
    assert sha(reference) == sha(stored) == sha(imported) == direct['file_sha256'] and stored_rows(imported) == expected
    original = archive.read_bytes()
    directory = int.from_bytes(original[-6:-2], 'little')
    for label, offsets, value, diagnostic in [
        ('crc', (14, directory+16), int.from_bytes(original[14:18], 'little') ^ 1, 'member CRC'),
        ('short-size', (22, directory+24), len(inputs['matrix.mtx'])-1, 'exceeds declared size'),
        ('long-size', (22, directory+24), len(inputs['matrix.mtx'])+1, 'expanded member size'),
        ('expanded-limit', (22, directory+24), 2**30, 'Expanded Matrix Market ZIP exceeds byte limit'),
        ('central-conflict', (directory+24,), len(inputs['matrix.mtx'])+1, 'central directory differs'),
    ]:
        damaged = bytearray(original)
        for offset in offsets:
            damaged[offset:offset+4] = value.to_bytes(4, 'little')
        source, destination = output/('deflate-'+label+'.zip'), output/('deflate-'+label+'.qmx')
        source.write_bytes(damaged)
        refused = run('archive-deflate-'+label, 'matrix-market-index-zip', source, destination, status=2)
        assert diagnostic in refused['error'] and not destination.exists()
    damaged = bytearray(original)
    payload_offset = 30 + len(names[0])
    damaged[payload_offset:payload_offset+5] = bytes(5)
    source, destination = output/'invalid-deflate.zip', output/'invalid-deflate.qmx'
    source.write_bytes(damaged)
    refused = run('archive-deflate-invalid', 'matrix-market-index-zip', source, destination, status=2)
    assert 'Invalid or truncated Matrix Market ZIP Deflate member' in refused['error'] and not destination.exists()
    assert sha(archive) == archive_hash
    reordered = check_archive_member_order(run, output, archive, inputs, imported)
    if descriptors:
        return check_streaming_archive(run, output, inputs, reordered)
    return reordered


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("host", "project", "output"):
        parser.add_argument("--" + key, type=lambda value: Path(value).resolve(), required=True)
    parser.add_argument("--image", default="1")
    parser.add_argument("--association")
    parser.add_argument("--reader-manifest", action="append", default=[],
                        type=lambda value: Path(value).resolve())
    parser.add_argument("--continuous-project", type=lambda value: Path(value).resolve())
    parser.add_argument("--reimport", type=lambda value: Path(value).resolve(),
                        help="Optional native SDK probe accepting MATRIX FEATURES BARCODES NEW_QMX")
    parser.add_argument("--require-edges", action="store_true",
                        help="Require an explicit zero, 2^53+1 and UINT64_MAX in the fixture")
    parser.add_argument("--spatial", action="store_true",
                        help="Restore positions and the re-indexed matrix on an empty saved image")
    parser.add_argument("--position-reexport", action="store_true",
                        help="Verify exact re-export from the restored point frame")
    parser.add_argument("--gzip", action="store_true",
                        help="Re-index gzip-wrapped Matrix Market and use it for spatial restore")
    parser.add_argument("--archive-index", action="store_true",
                        help="Check direct ZIP indexing against the unpacked inputs")
    parser.add_argument("--zip-deflate", action="store_true",
                        help="Verify Deflate exchange files against the same decoded inputs")
    parser.add_argument("--zip-descriptors", action="store_true",
                        help="Verify streaming ZIP32 data descriptors")
    parser.add_argument("--real-exchange", action="store_true",
                        help="Check continuous Matrix Market import, export and spatial replay")
    args = parser.parse_args()
    if args.position_reexport and not args.spatial:
        parser.error("--position-reexport requires --spatial")
    if args.gzip and args.reimport:
        parser.error("--gzip uses the native CLI rather than the SDK probe")
    if args.zip_deflate and not args.archive_index:
        parser.error("--zip-deflate requires --archive-index")
    if args.zip_descriptors and not args.zip_deflate:
        parser.error("--zip-descriptors requires --zip-deflate")
    if args.real_exchange and not args.continuous_project:
        parser.error("--real-exchange requires --continuous-project")
    args.output.mkdir(parents=True)
    env = {key: value for key, value in os.environ.items()
           if not key.upper().startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    if os.name == "nt":
        windows = Path(env["SYSTEMROOT"])
        env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    flags = [value for path in args.reader_manifest for value in ("--reader-plugins", path)]
    checks = {}

    def run(label, *command, status=0):
        result = subprocess.run([args.host, *flags, *command], env=env, cwd=args.output,
                                capture_output=True, encoding="utf-8", timeout=120)
        assert result.returncode == status, (label, result.stdout, result.stderr)
        receipt = json.loads(result.stdout if status == 0 else result.stderr)
        checks[label] = receipt
        return receipt

    listed = run("catalogue", "project-matrices", args.project, args.image)["matrices"]
    matrix, = [record for record in listed
               if args.association is None or record["association_id"] == args.association]
    association = matrix["association_id"]
    sidecar = Path(matrix["path"]).resolve()
    original = (sha(args.project), sha(sidecar))
    assert original[1] == matrix["file_sha256"]
    expected = stored_rows(sidecar)
    if args.require_edges:
        counts = {value for _, _, value in expected[2]}
        assert {0, 2**53 + 1, 2**64 - 1} <= counts

    archive = args.output / "counts.zip"
    export_args = ("project-matrix-export-mtx", args.project, args.image, association)
    receipt = run("export", *export_args, archive)
    assert receipt["state"] == "exported" and receipt["bytes"] == archive.stat().st_size
    assert receipt["matrix_file_sha256"] == original[1] and receipt["association_id"] == association
    inspect_archive(archive, expected)
    archive_sha = sha(archive)

    repeat = args.output / "repeat.zip"
    run("deterministic-export", *export_args, repeat)
    assert sha(repeat) == archive_sha
    refusal = run("existing-destination", *export_args, archive, status=2)
    assert "already exists" in refusal["error"] and sha(archive) == archive_sha
    missing_wal = sidecar.with_name(sidecar.name + "-wal")
    for label, destination in (("project-input", args.project), ("matrix-input", sidecar),
                               ("matrix-wal", missing_wal)):
        refusal = run(label, *export_args, destination, status=2)
        assert "must not replace its input" in refusal["error"], refusal
    assert not missing_wal.exists()
    if args.output.stat().st_dev == sidecar.stat().st_dev:
        alias = args.output / "matrix-inode.zip"
        os.link(sidecar, alias)
        refusal = run("hardlink-input", *export_args, alias, status=2)
        assert "aliases its input" in refusal["error"] and sha(alias) == original[1]
        alias.unlink()
    assert (sha(args.project), sha(sidecar)) == original

    if args.continuous_project:
        catalogue = run("continuous-catalogue", "project-matrices", args.continuous_project,
                        args.image)["matrices"]
        item, = [record for index, record in enumerate(catalogue)
                 if run(f"continuous-domain-{index}", "matrix-info", record["path"])["domain"] == "continuous"]
        destination = args.output / "continuous.zip"
        refusal = run("continuous-refused", "project-matrix-export-mtx", args.continuous_project, args.image,
                      item["association_id"], destination, status=2)
        assert "counts" in refusal["error"].lower(), refusal
        assert not destination.exists()
        if args.real_exchange:
            check_real_exchange(run, args.output, args.continuous_project, args.image, item)

    with ZipFile(archive) as source:
        for name in ("matrix.mtx", "features.tsv", "barcodes.tsv"):
            (args.output / name).write_bytes(source.read(name))
    imported = args.output / "reimported.qmx"
    if args.reimport:
        result = subprocess.run(
            [args.reimport, "matrix.mtx", "features.tsv", "barcodes.tsv", imported],
            env=env, cwd=args.output, capture_output=True, encoding="utf-8", timeout=120)
        assert result.returncode == 0, (result.stdout, result.stderr)
        checks["native-reimport"] = {"file_sha256": sha(imported)}
    else:
        inputs = [args.output / name for name in ("matrix.mtx", "features.tsv", "barcodes.tsv")]
        if args.gzip:
            archives = [path.with_suffix(path.suffix + ".gz") for path in inputs]
            for source, destination in zip(inputs, archives):
                destination.write_bytes(gzip.compress(source.read_bytes(), mtime=0))
            inputs = archives
        index = ("matrix-market-index", *inputs)
        receipt = run("native-reimport", *index, imported, "--catalogue", "tenx")
        assert receipt["state"] == "indexed" and receipt["file_sha256"] == sha(imported)
        assert receipt["input_profile"] == "tenx-tsv" and receipt["domain"] == "counts"
        reopened = run("reopened-index", "matrix-info", imported)
        assert all(receipt[key] == reopened[key] for key in
                   ("source_sha256", "file_sha256", "features", "observations", "entries", "input_profile"))
        if args.gzip:
            with closing(sqlite3.connect(imported.as_uri() + "?mode=ro&immutable=1", uri=True)) as database:
                recorded = dict(database.execute("SELECT key,value FROM metadata"))
            assert [recorded[key] for key in ("matrix_sha256", "features_sha256", "observations_sha256")] == [
                sha(path) for path in inputs]
            broken = args.output / "bad-crc.tsv.gz"
            payload = bytearray(inputs[1].read_bytes())
            payload[-8] ^= 1
            broken.write_bytes(payload)
            destination = args.output / "bad-crc.qmx"
            refusal = run("gzip-bad-trailer", "matrix-market-index", inputs[0], broken, inputs[2],
                          destination, "--catalogue", "tenx", status=2)
            assert "Invalid or truncated Matrix Market gzip" in refusal["error"] and not destination.exists()

        destination = args.output / "refused.qmx"
        run("missing-catalogue", *index, destination, status=2)
        run("unknown-catalogue", *index, destination, "--catalogue", "auto", status=2)
        run("wrong-catalogue", *index, destination, "--catalogue", "qpx", status=2)
        assert not destination.exists()
        old_digest = sha(imported)
        run("index-existing-destination", *index, imported, "--catalogue", "tenx", status=2)
        assert sha(imported) == old_digest

        wal = destination.with_name(destination.name + "-wal")
        wal.write_bytes(b"Reserved SQLite companion")
        refusal = run("index-existing-companion", *index, destination, "--catalogue", "tenx", status=2)
        assert "SQLite companion" in refusal["error"] and not destination.exists()
        assert wal.read_bytes() == b"Reserved SQLite companion"

        features, barcodes, counts = expected
        (args.output / "qpx-features.tsv").write_bytes(
            ("feature_id\n" + "".join(row[0] + "\n" for row in features)).encode("utf-8"))
        (args.output / "qpx-observations.tsv").write_bytes(
            ("observation_id\n" + "".join(name + "\n" for name in barcodes)).encode("utf-8"))
        qpx_output = args.output / "qpx-catalogue.qmx"
        qpx = run("native-qpx-index", "matrix-market-index", "matrix.mtx", "qpx-features.tsv",
                  "qpx-observations.tsv", qpx_output, "--catalogue", "qpx")
        assert qpx["input_profile"] == "qpx-tsv"
        assert stored_rows(qpx_output) == ([(row[0], row[0], "Gene Expression") for row in features], barcodes, counts)

        duplicate = args.output / "duplicate.mtx"
        duplicate.write_bytes(("%%MatrixMarket matrix coordinate integer general\n" +
                               f"{len(features)} {len(barcodes)} 2\n1 1 2\n1 1 3\n").encode("ascii"))
        duplicate_output = args.output / "duplicate.qmx"
        refusal = run("duplicate-coordinate", "matrix-market-index", duplicate, "features.tsv",
                      "barcodes.tsv", duplicate_output, "--catalogue", "tenx", status=2)
        assert "duplicate" in refusal["error"].lower() and not duplicate_output.exists()

    assert stored_rows(imported) == expected
    if args.archive_index:
        catalog = run("archive-capabilities", "--formats")
        capability, = [row for row in catalog["matrix"] if row["provider_id"] == "matrix-market"]
        assert capability["can_import_zip"] is True
        if args.gzip:
            assert capability["can_import_gzip"] is True
        plain = args.output / "unpacked.qmx"
        unpacked = run("unpacked-reference", "matrix-market-index", "matrix.mtx", "features.tsv",
                       "barcodes.tsv", plain, "--catalogue", "tenx")
        direct = args.output / "direct.qmx"
        zipped = run("archive-index", "matrix-market-index-zip", archive, direct)
        assert zipped["source_sha256"] == unpacked["source_sha256"]
        assert zipped["file_sha256"] == sha(direct) == sha(plain) and stored_rows(direct) == expected
        damaged = args.output / "bad-member.zip"
        payload = bytearray(archive.read_bytes())
        directory = int.from_bytes(payload[-6:-2], "little")
        # Change both copies of the first member's CRC while preserving its payload.
        payload[14] ^= 1
        payload[directory + 16] ^= 1
        damaged.write_bytes(payload)
        refused = args.output / "bad-member.qmx"
        status = run("archive-bad-crc", "matrix-market-index-zip", damaged, refused, status=2)
        assert "Invalid Matrix Market ZIP member CRC" in status["error"] and not refused.exists()
        if args.zip_deflate:
            assert capability["can_import_zip_deflate"] is True
            imported = check_deflated_archive(run, args.output, expected, args.zip_descriptors)

    if args.spatial:
        positions = args.output / "positions.tsv"
        position_receipt = run("export-positions", "project-matrix-export-positions", args.project,
                               args.image, association, positions)
        plane = position_receipt["plane"]
        frame = ("--frame-channel", str(plane["channel"]), "--frame-z", str(plane["z"]),
                 "--frame-time", str(plane["time"]))
        _, *rows = positions.read_text(encoding="utf-8").splitlines()
        coordinates = {row[0]: [float(row[1]), float(row[2])] for row in (line.split("\t") for line in rows)}
        assert len(coordinates) == len(expected[1]) and set(coordinates) == set(expected[1])

        restore = ("project-matrix-import-positions", args.project, args.image, imported)
        mismatch = args.output / "wrong-positions.tsv"
        mismatch.write_bytes(("observation_id\tx_px\ty_px\n" + "absent-from-matrix\t" + rows[0].split("\t", 1)[1] +
                              "\n" + "".join(line + "\n" for line in rows[1:])).encode("utf-8"))
        refused = args.output / "wrong-positions.qpx"
        run("wrong-position-id", *restore, mismatch, refused, *frame, status=2)
        assert not refused.exists()

        received = args.output / "received.qpx"
        imported_digest = sha(imported)
        saved = run("restore-spatial-project", *restore, positions, received, *frame)
        assert saved["state"] == "saved" and saved["owned"] and saved["observations"] == len(coordinates)
        received_digest = sha(received)
        run("restore-preserves-existing-project", *restore, positions, received, *frame, status=2)
        assert sha(received) == received_digest

        # Move only the authored outputs of this test to simulate losing the handoff inputs.
        offline = args.output / "offline-inputs"
        offline.mkdir()
        imported.rename(offline / imported.name)
        positions.rename(offline / positions.name)
        received_id = saved["matrix_source_sha256"]
        owned, = [item for item in run("restored-catalogue", "project-matrices", received, args.image)["matrices"]
                  if item["association_id"] == received_id]
        assert owned["owned"] and owned["file_sha256"] == imported_digest
        assert stored_rows(Path(owned["path"]).resolve()) == expected
        reexport = args.output / "reexport.zip"
        run("reexport-offline-original", "project-matrix-export-mtx", received, args.image, received_id, reexport)
        assert sha(reexport) == archive_sha
        geometry = args.output / "received.geojson"
        run("export-restored-points", "geojson-export", received, args.image, geometry)
        points = json.loads(geometry.read_bytes())["features"]
        assert len(points) == len(coordinates) and all(point["geometry"]["type"] == "Point" for point in points)
        assert {point["properties"]["name"]:point["geometry"]["coordinates"] for point in points} == coordinates
        expected_plane = dict(c=plane["channel"], z=plane["z"], t=plane["time"])
        for point in points:
            assert point["geometry"].get("plane", dict(c=-1, z=0, t=0)) == expected_plane
        if args.position_reexport:
            replay = args.output / "replayed-positions.tsv"
            export_positions = ("project-matrix-export-positions", received, args.image, received_id)
            selection = ("--frame-id", saved["coordinate_frame"], *frame)
            receipt = run("reexport-backed-positions", *export_positions, replay, *selection)
            assert receipt["coordinate_frame"] == saved["coordinate_frame"] and receipt["plane"] == plane
            assert receipt["matrix_file_sha256"] == imported_digest
            assert replay.read_bytes() == (offline / positions.name).read_bytes()
            run("backed-positions-existing-output", *export_positions, replay, *selection, status=2)
            assert replay.read_bytes() == (offline / positions.name).read_bytes()
            bad_frame = args.output / "refused-frame.tsv"
            run("backed-positions-wrong-frame", *export_positions, bad_frame,
                "--frame-id", "unrelated", *frame, status=2)
            assert not bad_frame.exists()
            assert sha(Path(owned["path"])) == imported_digest
        assert sha(received) == received_digest and (sha(args.project), sha(sidecar)) == original

    assert not list(args.output.glob("*.qpx-tmp-*"))
    assert not list(args.output.glob(".qpx-expression-*"))
    report = {"passed": True, "host_sha256": sha(args.host), "archive_sha256": archive_sha,
              "checks": checks}
    (args.output / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} exact Matrix Market export checks passed")


if __name__ == "__main__":
    main()
