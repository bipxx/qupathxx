"""Check native NPZ import and offline exact QMX reads."""
import argparse
import hashlib
from io import BytesIO
import json
import os
from pathlib import Path
import struct
import subprocess
import zipfile

import numpy as np


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("host", "manifest", "probe", "baseline", "output"):
        parser.add_argument("--" + key, type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir()
    manifest = args.manifest.resolve()
    provider, = json.loads(manifest.read_bytes())["plugins"]
    module = manifest.with_name(provider["module"]["path"])
    identity = (sha(module), sha(manifest))
    assert provider["module"]["sha256"] == identity[0] and provider["provider_id"] == "org.qupathxx.counts-npz"
    host, probe = args.host.resolve(), args.probe.resolve()
    env = os.environ.copy()
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {key: [] for key in ("indexed", "offline", "refusals")}
    source_dir = out / "sources"
    source_dir.mkdir()
    image = out / "image.ppm"
    image.write_bytes(b"P6\n8 6\n255\n" + bytes(8 * 6 * 3))
    positions_text = "observation_id\tx_px\ty_px\n1\t6\t4\n0001\t1.25\t2.5\n"
    positions = source_dir / "positions.tsv"
    positions.write_bytes(positions_text.encode())
    command = [host, "--format-plugins", manifest, "matrix-index", provider["provider_id"]]

    def run(cmd, code=0):
        result = subprocess.run(cmd, cwd=out, env=env, capture_output=True, text=True, timeout=30, encoding="utf-8")
        assert result.returncode == code, (cmd, result.returncode, result.stdout, result.stderr)
        return result.stdout, result.stderr

    def invoke(source, output, pos=positions):
        return command + [source, pos, image, output]

    def accept(name, source, profile):
        target = out / (name + ".qmx")
        stdout, _ = run(invoke(source, target))
        report = json.loads(stdout)
        assert report["features"] == 3 and report["observations"] == 2 and report["entries"] == 6
        assert report["file_sha256"] == sha(target) and report["receipt"]["source_sha256"] == sha(source)
        assert report["positions_sha256"] == sha(positions)
        assert report["receipt"]["provider"]["module_sha256"] == identity[0]
        checks["indexed"].append(name)
        return target, report["file_sha256"], profile

    baseline = args.baseline.resolve()
    assert sha(baseline) == "bd2411b10cb61122cef237d1a7c42fd0064ab58fb38b68451d9dc07708d3ee6a"
    retained = source_dir / "retained.npz"
    retained.write_bytes(baseline.read_bytes())
    counts = np.array([[2**53 + 1, 0, 2**64 - 1], [17, 53, 0]], dtype=">u8", order="F")
    obs = np.array(["0001", "1"])
    features = np.array(["G-01", "G1", "\u03bc-feature"], dtype=">U9")
    with np.load(retained, allow_pickle=False) as oracle:
        assert np.array_equal(oracle["counts"], counts) and oracle["counts"].flags.f_contiguous
        assert np.array_equal(oracle["observation_ids"], obs) and np.array_equal(oracle["feature_ids"], features)
    offline = [accept("retained", retained, "wide")]

    byte_ids = np.array([b"0001", b"1"])
    signed = np.array([[9, 0, 200], [17, 53, 0]], dtype="<i2")

    def produce(name, values=counts, observations=byte_ids, columns=features, **other):
        path = source_dir / (name + ".npz")
        np.savez(path, counts=values, observation_ids=observations, feature_ids=columns, **other)
        return path

    stored = produce("stored", signed)
    offline.append(accept("stored", stored, "small"))

    wide = source_dir / "wide-classic.zip"
    with zipfile.ZipFile(wide, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, value, version in (("feature_ids", features, (3, 0)), ("counts", counts, (2, 0)),
                                     ("observation_ids", obs, (1, 0))):
            stream = BytesIO()
            np.lib.format.write_array(stream, value, version=version, allow_pickle=False)
            archive.writestr(name + ".npy", stream.getvalue())
    offline.append(accept("classic", wide, "wide"))

    def refuse(name, source, diagnostic, pos=positions):
        target = out / (name + ".qmx")
        before = sha(source)
        _, stderr = run(invoke(source, target, pos), 2)
        assert diagnostic in stderr and sha(source) == before and not target.exists(), (name, stderr)
        checks["refusals"].append(name)

    refuse("overflow-shape", produce("shape", values=counts[:1]), "axis lengths differ")
    refuse("duplicate-observation", produce("duplicate", observations=np.array([b"1", b"1"])), "duplicated")
    refuse("wrong-type", produce("floating", values=signed.astype("f8")), "fixed-width integer")
    refuse("negative", produce("negative", values=-signed), "nonnegative")
    refuse("empty-id", produce("empty", observations=np.array([b"", b"1"])), "ID is empty")
    refuse("bad-utf8", produce("bad-utf8", observations=np.array([b"\xff", b"1"])), "Invalid UTF-8")
    refuse("extra", produce("extra", junk=np.zeros(1)), "exactly three")
    damaged = bytearray(stored.read_bytes())
    start = 30 + struct.unpack_from("<H", damaged, 26)[0] + struct.unpack_from("<H", damaged, 28)[0]
    damaged[start + 2] ^= 1
    crc = source_dir / "bad-crc.npz"
    crc.write_bytes(damaged)
    refuse("crc", crc, "checksum differs")
    damaged = bytearray(retained.read_bytes())
    start = 30 + struct.unpack_from("<H", damaged, 26)[0] + struct.unpack_from("<H", damaged, 28)[0]
    damaged[start] = (damaged[start] & ~6) | 6
    deflate = source_dir / "bad-deflate.npz"
    deflate.write_bytes(damaged)
    refuse("deflate", deflate, "deflate member is damaged")
    missing = source_dir / "missing.tsv"
    missing.write_bytes(positions_text.splitlines(keepends=True)[0].encode() + b"1\t6\t4\n")
    refuse("position-join", retained, "position", missing)

    source_dir.rename(out / "unavailable-sources")
    for target, digest, profile in offline:
        stdout, _ = run([host, "matrix-info", target])
        assert json.loads(stdout)["file_sha256"] == digest
        stdout, _ = run([probe, target, digest, profile])
        checks["offline"].append(stdout.strip())
    assert identity == (sha(module), sha(manifest))
    (out / "validation.json").write_text(json.dumps({"passed": True, "module_sha256": identity[0],
          "manifest_sha256": identity[1], "baseline_sha256": sha(baseline), **checks}, indent=2) + "\n", encoding="utf-8")
    print({key: len(value) for key, value in checks.items()})


if __name__ == "__main__":
    main()
