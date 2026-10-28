"""File-format dispatch must preserve UTF-8 bytes and the caller's C locale."""
import os
from pathlib import Path
import subprocess
import sys

import pytest


@pytest.mark.parametrize('ctype', ['C', *(
    ['English_United States.1252', 'tr-TR'] if os.name == 'nt' else []
)])
def test_unicode_reader_dispatch_and_nominal_resume(tmp_path, ctype):
    # Each locale has its own process: LC_CTYPE is global native state.
    program = r'''
import locale
from pathlib import Path
import sys
import qupathxx as q
from test_nominal_cell_batches import test_nominal_linked_pixel_change_invalidates_resume

selected = locale.setlocale(locale.LC_CTYPE, sys.argv[1])
root = Path(sys.argv[2])
for index, suffix in enumerate(['ppm', 'PPM']):
    source = root / ('IMAGE-\u539f\u59cb-\u0391\u03a3-' + str(index) + '.' + suffix)
    source.write_bytes(b'P6\n2 1\n255\n\x01\x02\x03\x04\x05\x06')
    original = str(source)
    direct = q.Image.read_ppm(original)
    project = q.Project()
    image = project.add_image(original)
    explicit = project.add_image(original, reader='ppm')
    assert project.document(image).metadata.width == direct.width == 2
    assert project.document(explicit).metadata.width == 2
    saved = root / ('PROJECT-\u539f\u59cb-' + str(index) + '.qpxproj')
    project.save(str(saved))
    reopened = q.Project.open(str(saved))
    assert reopened.document(image).metadata.width == 2
    assert str(source) == original
test_nominal_linked_pixel_change_invalidates_resume(root)
assert locale.setlocale(locale.LC_CTYPE) == selected
print('UTF-8 auto/explicit readers, uppercase extension, project reopen and changed-source resume passed:', selected)
'''
    env = os.environ.copy()
    env['PYTHONPATH'] = str(Path(__file__).resolve().parent)
    result = subprocess.run(
        [sys.executable, '-X', 'utf8', '-c', program, ctype, str(tmp_path)],
        env=env, capture_output=True, encoding='utf-8', timeout=120,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'changed-source resume passed:' in result.stdout
