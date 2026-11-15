#!/usr/bin/env python3
"""Development-only literal WebP lossless oracles and native plugin controls."""
import argparse
import hashlib
from io import BytesIO
import json
from pathlib import Path
import shutil
import struct
import subprocess


def chunk(tag, payload):
    return tag + struct.pack("<I", len(payload)) + payload + (b"\0" if len(payload) % 2 else b"")


def container(chunks):
    body = b"WEBP" + b"".join(chunks)
    return b"RIFF" + struct.pack("<I", len(body)) + body


def lossless_payload(data):
    assert data[:4] == b"RIFF" and data[8:16] == b"WEBPVP8L"
    size = struct.unpack_from("<I", data, 16)[0]
    assert struct.unpack_from("<I", data, 4)[0] == len(data) - 8
    assert len(data) == 20 + size + size % 2
    assert not size % 2 or data[-1] == 0
    payload = data[20:20 + size]
    assert payload[0] == 0x2f and payload[4] >> 5 == 0
    return payload


def write_fixtures(directory):
    from PIL import Image, features, __version__ as pillow_version
    directory.mkdir(parents=True, exist_ok=True)
    cases, negatives = [], {}

    def encoded(width, height, rgba, method=6, mode="RGBA"):
        assert len(rgba) == width * height * 4
        image = Image.frombytes("RGBA", (width, height), rgba)
        if mode != "RGBA":
            assert rgba[3::4] == bytes([255]) * (width * height)
            image = image.convert(mode)
        buffer = BytesIO()
        image.save(buffer, "WEBP", lossless=True, exact=True, method=method)
        data = buffer.getvalue()
        lossless_payload(data)
        decoded = Image.open(BytesIO(data))
        decoded.load()
        assert decoded.size == (width, height) and decoded.convert("RGBA").tobytes() == rgba
        return data

    def add(name, width, height, rgba, *, method=6, mode="RGBA", data=None, hint_seed=None):
        rgba = bytes(rgba)
        data = encoded(width, height, rgba, method, mode) if data is None else data
        payload = lossless_payload(data)
        expected_hint = (payload[4] >> 4) & 1
        decoded = Image.open(BytesIO(data))
        decoded.load()
        adapter_exact = decoded.convert("RGBA").tobytes() == rgba
        if hint_seed is None:
            assert adapter_exact, name
        else:
            seed_path = directory / (hint_seed + ".webp")
            seed = seed_path.read_bytes()
            assert len(seed) == len(data) and seed[:24] == data[:24] and seed[25:] == data[25:] and seed[24] ^ data[24] == 0x10
            assert (directory / (hint_seed + ".raw")).read_bytes() == rgba
            if not adapter_exact:
                assert decoded.mode == "RGB" and expected_hint == 0
                assert decoded.tobytes() == bytes(v for i, v in enumerate(rgba) if i % 4 != 3)
        (directory / (name + ".webp")).write_bytes(data)
        (directory / (name + ".raw")).write_bytes(rgba)
        (directory / (name + ".rgba")).write_bytes(rgba)
        if all(value == 255 for value in rgba[3::4]):
            (directory / (name + ".rgb")).write_bytes(bytes(v for i, v in enumerate(rgba) if i % 4 != 3))
        cases.append({"name": name, "width": width, "height": height, "channels": 4,
                      "method": method, "encoder_mode": mode, "alpha_hint": expected_hint,
                      "payload_bytes": len(payload), "odd_zero_padding": bool(len(payload) % 2),
                      "source_sha256": hashlib.sha256(data).hexdigest(), "raw_sha256": hashlib.sha256(rgba).hexdigest(),
                      "rgba_sha256": hashlib.sha256(rgba).hexdigest(), "pillow_adapter_exact": adapter_exact,
                      "hint_only_seed": hint_seed,
                      "oracle_basis": "Authored literal RGBA; hint-only variant retains the exactly Pillow-gated seed's literal samples" if hint_seed else "Authored literal RGBA plus exact Pillow adapter roundtrip"})
        return data

    rgb = [0, 1, 2, 9, 10, 32, 35, 127, 128, 254, 255, 64, 21, 46, 201,
           73, 99, 133, 200, 17, 240, 3, 222, 88, 155, 111, 42, 31, 63, 95,
           4, 8, 16, 32, 64, 128, 127, 129, 253, 89, 144, 233, 255, 0, 255]
    opaque = bytes(v for i in range(15) for v in (*rgb[i * 3:i * 3 + 3], 255))
    alpha_levels = (0, 1, 127, 254, 255, 0, 17, 128, 255, 255, 2, 253, 255, 0, 255)
    alpha = bytearray(v for i in range(15) for v in (*rgb[i * 3:i * 3 + 3], alpha_levels[i]))
    alpha[:4] = bytes((17, 33, 65, 0))
    base = add("rgba-main", 5, 3, opaque)
    alpha_base = add("rgba-alpha", 5, 3, alpha)
    add("rgba-source-changed", 6, 3, opaque + bytes((17, 65, 201, 255)) * 3, method=0)
    add("rgb-input", 1, 1, bytes((17, 33, 65, 255)), mode="RGB")
    add("opaque-solid-m0", 5, 3, bytes((17, 33, 65, 255)) * 15, method=0)
    add("opaque-single", 1, 1, bytes((241, 129, 5, 255)))
    gradient = bytes(v for y in range(11) for x in range(17) for v in ((x * 17 + y * 31) & 255, (x * 73 + y * 13) & 255, (x * 7 + y * 97) & 255, 255))
    add("opaque-gradient-m0", 17, 11, gradient, method=0)
    add("opaque-gradient-m6", 17, 11, gradient)
    add("transparent-solid", 5, 3, bytes((17, 33, 65, 0)) * 15, method=0)
    add("transparent-multicolor", 5, 3, bytes(v if i % 4 != 3 else 0 for i, v in enumerate(opaque)))
    ramp = bytes(v for y in range(11) for x in range(17) for v in ((x * 19 + y * 43) & 255, (x * 83 + y * 29) & 255, (x * 11 + y * 71) & 255, (0, 1, 2, 17, 63, 64, 127, 128, 191, 253, 254, 255)[(x + y) % 12]))
    add("alpha-ramp", 17, 11, ramp)
    colors = ((17, 33, 65, 0), (241, 129, 5, 1), (0, 255, 128, 128), (255, 254, 253, 255))
    palette = bytes(v for y in range(11) for x in range(17) for v in colors[(x + 3 * y) % 4])
    add("few-colors-m0", 17, 11, palette, method=0)
    add("few-colors-m6", 17, 11, palette)
    add("gray-input", 7, 3, bytes(v for y in range(3) for x in range(7) for v in (((x * 37 + y * 59) & 255),) * 3 + (255,)), mode="L")
    add("channel-correlated", 17, 11, bytes(v for y in range(11) for x in range(17) for v in ((x + y * 17) & 255, (x + y * 17 + 1) & 255, (2 * (x + y * 17)) & 255, 255)))
    random = bytearray()
    state = 0x13579bdf
    for _ in range(17 * 11 * 4):
        state = (1664525 * state + 1013904223) & 0xffffffff
        random.append(state >> 24)
    add("literal-noise-m0", 17, 11, random, method=0)
    add("literal-noise-m6", 17, 11, random)
    add("opaque-hint-set", 5, 3, opaque, data=base[:24] + bytes((base[24] | 0x10,)) + base[25:], hint_seed="rgba-main")
    add("alpha-hint-cleared", 5, 3, alpha, data=alpha_base[:24] + bytes((alpha_base[24] & ~0x10,)) + alpha_base[25:], hint_seed="rgba-alpha")
    odd = next((c for c in cases if c["odd_zero_padding"]), None)
    assert odd is not None, "Authored profiles must exercise legal odd-size chunk padding"
    odd_data = (directory / (odd["name"] + ".webp")).read_bytes()

    def bad(name, content, status):
        (directory / (name + ".webp")).write_bytes(content)
        negatives[name] = {"status": status, "sha256": hashlib.sha256(content).hexdigest()}

    payload = lossless_payload(base)
    bad("riff-signature", b"RIFX" + base[4:], 2)
    bad("webp-signature", base[:8] + b"NOPE" + base[12:], 2)
    bad("missing-image-chunk", container([]), 4)
    bad("short-chunk-header", container([b"VP8L"]), 4)
    bad("short-lossless-header", container([chunk(b"VP8L", payload[:4])]), 4)
    for name, declared in (("riff-size-small", len(base) - 9), ("riff-size-large", len(base) - 7)):
        bad(name, base[:4] + struct.pack("<I", declared) + base[8:], 4)
    bad("chunk-size-overshoot", base[:16] + struct.pack("<I", len(payload) + 4) + base[20:], 4)
    bad("chunk-size-underextent", base[:16] + struct.pack("<I", len(payload) - 2) + base[20:], 4)
    bad("nonzero-padding", odd_data[:-1] + b"\x7f", 4)
    bad("bad-lossless-signature", container([chunk(b"VP8L", b"\0" + payload[1:])]), 4)
    bad("unsupported-lossless-version", container([chunk(b"VP8L", payload[:4] + bytes((payload[4] | 0x20,)) + payload[5:])]), 2)
    bad("missing-compressed-payload", container([chunk(b"VP8L", payload[:5])]), 4)
    bad("duplicate-vp8l", container([chunk(b"VP8L", payload), chunk(b"VP8L", payload)]), 4)
    bad("truncated", base[:-1], 4)
    bad("trailing", container([chunk(b"VP8L", payload), b"\0\0"]), 4)
    bad("concatenated", base + base, 4)
    for name, tag in (("alph", b"ALPH"), ("anim", b"ANIM"), ("anmf", b"ANMF"), ("icc", b"ICCP"), ("exif", b"EXIF"), ("xmp", b"XMP "), ("unknown", b"QPX!")):
        bad(name, container([chunk(tag, b"Authored"), chunk(b"VP8L", payload)]), 2)
    vp8x = bytes(4) + (4).to_bytes(3, "little") + (2).to_bytes(3, "little")
    bad("vp8x-static-wrapper", container([chunk(b"VP8X", vp8x), chunk(b"VP8L", payload)]), 2)
    lossy = BytesIO()
    Image.frombytes("RGBA", (5, 3), opaque).convert("RGB").save(lossy, "WEBP", lossless=False, quality=80)
    check_lossy = Image.open(BytesIO(lossy.getvalue()))
    check_lossy.load()
    assert check_lossy.size == (5, 3) and lossy.getvalue()[12:16] == b"VP8 "
    bad("lossy-vp8", lossy.getvalue(), 2)
    animation = BytesIO()
    Image.frombytes("RGBA", (5, 3), opaque).save(animation, "WEBP", save_all=True,
        append_images=[Image.frombytes("RGBA", (5, 3), bytes((17, 65, 201, 255)) * 15)], duration=100, loop=0, lossless=True, exact=True)
    check_animation = Image.open(BytesIO(animation.getvalue()))
    assert check_animation.n_frames == 2
    bad("animation", animation.getvalue(), 2)
    huge = (struct.unpack_from("<I", payload, 1)[0] & 0xf0000000) | 0x0fffffff
    bad("decoded-size-bound", container([chunk(b"VP8L", payload[:1] + struct.pack("<I", huge) + payload[5:])]), 3)

    resources = {}
    for name, size, budget in (("budget-raster", 256, 32768), ("budget-scratch", 128, 98304)):
        rgba = bytes((17, 33, 65, 255)) * (size * size)
        data = encoded(size, size, rgba)
        path = directory / (name + ".webp")
        path.write_bytes(data)
        margin = 8192 + 8 * len(str(path.resolve()).encode("utf8"))
        assert len(data) + margin < budget
        if name == "budget-raster":
            assert size * size * 4 > budget
        else:
            assert len(data) + margin + size * size * 4 < budget
        resources[name] = {"filename": name + ".webp", "width": size, "height": size,
                           "encoded_bytes": len(data), "decoded_rgba_bytes": len(rgba), "conservative_wrapper_bytes": margin,
                           "work_budget": budget, "sha256": hashlib.sha256(data).hexdigest(), "pillow_exact": True}
    unicode_name = "WebP-\u03bc-\u56fe.webp"
    (directory / unicode_name).write_bytes(base)
    manifest = {"schema": "qpx.webp-test-fixtures/1", "cases": cases, "negative": negatives,
                "oracle": {"Pillow": pillow_version, "libwebp": features.version("webp"),
                           "basis": "Literal authored RGBA is the authority; lossless=True/exact=True Pillow roundtrip gates encoder output. Pillow uses libwebp and is not a different codec algorithm.",
                           "hint_adapter_limitation": "Pillow chooses RGB mode from the alpha hint; cleared-hint variants retain literal seed alpha and are not judged against Pillow's alpha-discarding adapter output."},
                "unicode_filename": {"path": unicode_name, "control": "rgba-main", "sha256": hashlib.sha256(base).hexdigest()},
                "resource_fixtures": resources}
    (directory / "fixtures.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")
    (directory / "fixtures.tsv").write_text("".join(f"{c['name']}\t{c['width']}\t{c['height']}\n" for c in cases), encoding="utf8")
    (directory / "negative.tsv").write_text("".join(f"{name}\t{row['status']}\n" for name, row in negatives.items()), encoding="utf8")
    return manifest

def run(command, directory, name, rows):
    result = subprocess.run(command, cwd=directory, capture_output=True, text=True, encoding="utf8", errors="strict")
    log = directory / (name + ".log")
    log.write_text(result.stdout + result.stderr, encoding="utf8")
    rows.append({"name": name, "command": command, "exit": result.returncode, "log": str(log)})
    if result.returncode:
        raise RuntimeError(f"{name} failed: {result.returncode}\n{result.stdout}{result.stderr}")


def validate(args):
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        raise ValueError("Validation requires a new or empty output directory")
    output.mkdir(parents=True, exist_ok=True)
    fixtures = output / "fixtures"
    fixture_manifest = write_fixtures(fixtures)
    rows = []
    artifacts = {name: str(getattr(args, name).resolve()) if getattr(args, name) else None
                 for name in ("module", "abi_test", "consumer", "region", "manifest")}

    def hashes():
        result = {}
        for name, path in artifacts.items():
            try:
                result[name] = hashlib.sha256(Path(path).read_bytes()).hexdigest() if path else None
            except OSError as error:
                result[name] = {"error": str(error)}
        return result

    before = hashes()
    assert all(value is None or isinstance(value, str) for value in before.values()), before
    report = {"schema": "qpx.webp-plugin-validation/1", "fixture_cases": len(fixture_manifest["cases"]),
              "negative_cases": len(fixture_manifest["negative"]), "checks": rows,
              "artifact_paths": artifacts, "artifacts_before": before,
              **{name + "_sha256": value for name, value in before.items()}}
    try:
        run([str(args.abi_test.resolve()), str(args.module.resolve()), str(fixtures)], output, "direct-abi", rows)
        if args.consumer and args.manifest:
            work = output / "project"
            work.mkdir()
            consumer = [str(args.consumer.resolve())]
            for mode in ("create", "reopen", "missing"):
                run(consumer + [mode, str(args.manifest.resolve()), str(fixtures), str(work)], output, "project-" + mode, rows)

            def stage_manifest(directory, *, overlay=False):
                directory.mkdir()
                changed = json.loads(args.manifest.read_text(encoding="utf8"))
                for descriptor in changed["plugins"]:
                    target = directory / descriptor["module"]["path"]
                    target.parent.mkdir(parents=True, exist_ok=True)
                    data = (args.manifest.parent / descriptor["module"]["path"]).read_bytes()
                    if overlay:
                        data += b"QPX WebP inert implementation identity check\n"
                    target.write_bytes(data)
                    descriptor["module"]["sha256"] = hashlib.sha256(data).hexdigest()
                    for dependency in descriptor["dependencies"]:
                        destination = directory / dependency["path"]
                        destination.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copy2(args.manifest.parent / dependency["path"], destination)
                manifest = directory / "reader-plugins.json"
                manifest.write_text(json.dumps(changed, indent=2) + "\n", encoding="utf8")
                return manifest

            changed_manifest = stage_manifest(output / "changed-package", overlay=True)
            run(consumer + ["changed-implementation", str(changed_manifest), str(fixtures), str(work)], output, "project-changed-implementation", rows)
            run(consumer + ["changed-source", str(args.manifest.resolve()), str(fixtures), str(work)], output, "project-changed-source", rows)
        if args.region and args.manifest:
            run([str(args.region.resolve()), "--reader-plugins", str(args.manifest.resolve()), "--readers"], output, "host-catalog", rows)
            run([str(args.region.resolve()), "--reader-plugins", str(args.manifest.resolve()), "--reader", "org.qupathxx.webp", str(fixtures / "rgba-main.webp"), "0", "0", "0", "5", "3", str(output / "host-region.pam")], output, "host-region", rows)
            pam = (output / "host-region.pam").read_bytes()
            end = pam.index(b"ENDHDR\n") + len(b"ENDHDR\n")
            assert pam[end:] == (fixtures / "rgba-main.rgba").read_bytes(), "Host PAM differs from independent exact WebP oracle"
        report["checks_passed"] = True
    finally:
        report["artifacts_after"] = hashes()
        report["artifacts_unchanged"] = before == report["artifacts_after"]
        report["passed"] = bool(report.get("checks_passed") and report["artifacts_unchanged"])
        (output / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf8")
        if not report["artifacts_unchanged"]:
            raise RuntimeError("Validation input module/executable/manifest bytes changed; see validation.json")
    print(json.dumps({"passed": True, "fixture_cases": len(fixture_manifest["cases"]), "native_checks": len(rows), "report": str(output / "validation.json")}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="mode", required=True)
    fixtures = commands.add_parser("fixtures")
    fixtures.add_argument("output", type=Path)
    validation = commands.add_parser("validate")
    for argument in ("module", "abi-test", "output"):
        validation.add_argument("--" + argument, required=True, type=Path)
    for argument in ("consumer", "manifest", "region"):
        validation.add_argument("--" + argument, type=Path)
    args = parser.parse_args()
    if args.mode == "fixtures":
        manifest = write_fixtures(args.output.resolve())
        print(json.dumps({"cases": len(manifest["cases"]), "negatives": len(manifest["negative"]), "output": str(args.output.resolve())}))
    else:
        validate(args)


if __name__ == "__main__":
    main()
