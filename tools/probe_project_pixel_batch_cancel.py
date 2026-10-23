"""Interrupt a real native pixel batch after it creates an owned output bundle."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import time
import uuid

import numpy as np
import qupathxx as q
from qupathxx import _native


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--work', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = args.work or Path('build/pixel-batch-interrupt')/uuid.uuid4().hex
    root.mkdir(parents=True, exist_ok=False)
    fixture = Path(__file__).resolve().parents[1]/'tests/data/pixel-streams/rectangle-v1-windows.qps'
    model = q.PixelStreamFile(str(fixture)).read_block(0).model
    doc = q.Document(q.Image.from_numpy(np.full((2048, 2048, 3), 128, np.uint8)))
    if model.resolution.pixel_width_um is not None:
        doc.set_calibration(model.resolution.pixel_width_um, model.resolution.pixel_height_um)
    doc.add_rectangle(q.Rect(0, 0, 2048, 2048))
    project = q.Project()
    project.add_document(doc)
    source = root/'source.qpxproj'
    project.save(str(source))
    recipe = root/'model.qpc'
    q.save_pixel_classifier(model, str(recipe))
    original = sha(source)
    recipe_hash = sha(recipe)
    executable = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    output = root/'output'
    flags = {'creationflags': subprocess.CREATE_NEW_PROCESS_GROUP} if os.name == 'nt' else {}
    start = time.monotonic()
    child = subprocess.Popen([str(executable), str(source), str(output), '--kind', 'pixels',
                              '--pixel-model', str(recipe), '--block-size', '16', '--tile-size', '16',
                              '--pixel-work-bytes', str(8*1024*1024)],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8', **flags)
    try:
        deadline = time.monotonic()+30
        while not any(path.parent.name.startswith('pixels-') for path in output.rglob('owner')):
            assert child.poll() is None, 'worker ended before creating its output bundle'
            assert time.monotonic() < deadline, 'worker did not reach output generation'
            time.sleep(.005)
        while not any(output.rglob('*.qps.qpx-tmp-*')):
            assert child.poll() is None, 'worker ended before opening its stream temporary'
            assert time.monotonic() < deadline, 'worker did not open its stream output'
            time.sleep(.005)
        child.send_signal(signal.CTRL_BREAK_EVENT if os.name == 'nt' else signal.SIGINT)
        stdout, stderr = child.communicate(timeout=25)
        assert child.returncode == 130, stdout+stderr
        report = json.loads(stdout.splitlines()[-1])
        assert report['state'] == 'cancelled' and report['saved'] == 0
        assert not list(output.rglob('*.qps')) and not list(output.rglob('result.qpxproj'))
        assert not list(output.rglob('pixels-*')) and not list(output.glob('.qupathxx-batch-work-*'))
        assert not list(output.rglob('*.qpx-tmp-*'))
        assert sha(source) == original and sha(recipe) == recipe_hash
    finally:
        if child.poll() is None:
            child.kill()
            child.communicate(timeout=10)
    proof = dict(schema='qpx.project-pixel-batch-interrupt/1', exit_code=child.returncode,
                 signal='CTRL_BREAK' if os.name == 'nt' else 'SIGINT', observed_owned_bundle_before_signal=True,
                 observed_stream_temporary_before_signal=True,
                 elapsed_ms=(time.monotonic()-start)*1000, source_sha256=original, model_file_sha256=recipe_hash,
                 executable_sha256=sha(executable), native_sha256=sha(_native.__file__),
                 source_tree_sha256=q.build_environment().source_tree_sha256, report=report,
                 no_incomplete_outputs=True, source_and_model_unchanged=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(proof, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(proof, indent=2))


if __name__ == '__main__':
    main()
