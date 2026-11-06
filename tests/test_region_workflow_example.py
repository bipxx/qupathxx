"""Observable example behavior, including a valid empty tissue result."""
import csv
import json
from pathlib import Path
import subprocess
import sys


EXAMPLE = Path(__file__).resolve().parents[1] / 'examples/region_workflow.py'


def test_white_roi_exports_zero_component_summary_and_preserves_existing_output(tmp_path):
    image = tmp_path / 'white.ppm'
    image.write_bytes(b'P6\n16 16\n255\n' + bytes([255, 255, 255]) * 256)
    output = tmp_path / 'output'
    command = [sys.executable, '-I', str(EXAMPLE), str(image), str(output)]
    done = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert done.returncode == 0, done.stdout + done.stderr
    summary = json.loads((output / 'workflow.json').read_text(encoding='utf-8'))
    assert summary['tissue_components'] == summary['tissue_foreground_pixels'] == 0
    assert summary['saved_replay'] and not summary['whole_slide_tissue']
    with (output / 'tissue.csv').open(newline='') as stream:
        rows = list(csv.DictReader(stream))
    assert len(rows) == 1 and rows[0]['component_samples'] == ''
    before = {p.name: p.read_bytes() for p in output.iterdir() if p.is_file()}
    refused = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert refused.returncode == 2
    assert {p.name: p.read_bytes() for p in output.iterdir() if p.is_file()} == before


def test_whole_slide_requires_a_real_coarse_level_and_creates_no_outputs(tmp_path):
    image = tmp_path / 'white.ppm'
    image.write_bytes(b'P6\n16 16\n255\n' + bytes([255, 255, 255]) * 256)
    output = tmp_path / 'invalid'
    done = subprocess.run([sys.executable, '-I', str(EXAMPLE), str(image), str(output),
                           '--whole-slide-tissue'], capture_output=True, text=True, timeout=30)
    assert done.returncode == 2 and 'native pyramid level above zero' in done.stderr
    assert not output.exists()
