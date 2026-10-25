"""Installed executable emits a proposal; existing project import performs apply."""
import json
import pytest
import qupathxx as q
from test_qptma_project import cli, no_temporaries
from test_tma_dearray import CONTRACT, scene

pytestmark = pytest.mark.skipif(not q.has_project, reason='requires native project storage')


def source_project(tmp_path, calibration=None):
    doc, _ = scene(CONTRACT['cases'][1])
    if calibration is not None:
        doc.set_calibration(*calibration)
    project = q.Project()
    project.add_document(q.Document(q.Image(16, 16, [255]*16*16*3)), 'Untouched')
    image = project.add_document(doc, 'Explicit inactive image')
    project.active_image = 1
    source = tmp_path/'source.qpxproj'
    project.save(str(source))
    return source, image


def arguments(source, output, image):
    return ['--detect-tma', str(source), str(output), '--image', str(image), '--rows', '3', '--columns', '4', '--core-width', '32', '--core-height', '32']


def test_cli_preview_then_explicit_project_import(tmp_path):
    source, image = source_project(tmp_path)
    before = source.read_bytes()
    output = tmp_path/'proposal.qptma'
    result = cli(*arguments(source, output, image))
    assert result.returncode == 0, result.stderr
    report = json.loads(result.stdout)
    assert report['schema'] == 'qpx.tma-dearray-preview/1' and report['image_id'] == image
    assert report['candidates'] == 11 and len(report['input_sha256']) == len(report['recipe_sha256']) == 64
    assert q.load_qptma(str(output)).cores[5].missing
    assert source.read_bytes() == before and q.Project.open(str(source)).document(image).tma_core_count == 0
    applied = tmp_path/'applied.qpxproj'
    result = cli('--import-qptma', source, output, applied, '--image', image)
    assert result.returncode == 0, result.stderr
    project = q.Project.open(str(applied))
    assert project.document(image).tma_core_count == 12 and project.document(1).tma_core_count == 0
    assert source.read_bytes() == before
    no_temporaries(tmp_path)


@pytest.mark.parametrize('flags', [[], ['--rows', '3'], ['--polarity', 'invalid'], ['--work-bytes', '1'], ['--source-pixels', '1'], ['--core-height', 'nan']])
def test_cli_rejects_bad_or_duplicate_options_without_output(tmp_path, flags):
    source, image = source_project(tmp_path)
    before = source.read_bytes()
    output = tmp_path/'refused.qptma'
    args = arguments(source, output, image)
    if not flags:
        args = args[:5]+args[7:]  # Missing required rows.
    elif flags[0] == "--core-height":
        args[args.index("--core-height")+1] = flags[1]
    else:
        args += flags
    result = cli(*args)
    assert result.returncode != 0 and not output.exists() and source.read_bytes() == before
    no_temporaries(tmp_path)


def test_cli_preserves_existing_output_and_source_alias(tmp_path):
    source, image = source_project(tmp_path)
    before = source.read_bytes()
    output = tmp_path/'existing.qptma'
    output.write_bytes(b'preserve me')
    assert cli(*arguments(source, output, image)).returncode != 0 and output.read_bytes() == b'preserve me'
    assert cli(*arguments(source, source, image)).returncode != 0 and source.read_bytes() == before
    no_temporaries(tmp_path)


@pytest.mark.parametrize('calibration', [(.5, .25), None])
def test_cli_physical_units_use_persisted_document_calibration(tmp_path, calibration):
    source, image = source_project(tmp_path, calibration)
    before = source.read_bytes()
    output = tmp_path/'physical.qptma'
    args = arguments(source, output, image)
    args[args.index('--core-width')+1] = '16'
    args[args.index('--core-height')+1] = '8'
    result = cli(*args, '--core-units', 'um')
    assert source.read_bytes() == before
    if calibration is None:
        assert result.returncode == 1 and not output.exists() and 'calibration is unknown' in result.stderr
    else:
        assert result.returncode == 0, result.stderr
        size = json.loads(result.stdout)['core_size']
        assert size == dict(units='um', width=16, height=8, width_pixels=32, height_pixels=32, pixel_width_um=.5, pixel_height_um=.25)
        assert q.load_qptma(str(output)).cores[5].missing
    no_temporaries(tmp_path)
