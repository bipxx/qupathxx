"""Installed executable emits a proposal; existing project import performs apply."""
import json
import pytest
import qupathxx as q
from test_qptma_project import cli, no_temporaries, values
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
    exported = tmp_path/'applied.qptma'
    result = cli('--export-qptma', applied, exported, '--image', image)
    assert result.returncode == 0, result.stderr
    assert values(q.load_qptma(str(exported))) == values(q.load_qptma(str(output)))
    untouched = tmp_path/'untouched.qptma'
    result = cli('--export-qptma', applied, untouched, '--image', 1)
    assert result.returncode == 1 and 'selected image has no TMA grid' in result.stderr
    assert not untouched.exists()
    assert source.read_bytes() == before
    no_temporaries(tmp_path)


def test_cli_detect_tma_requires_review_for_replaced_verified_slide(tmp_path):
    import numpy as np
    from test_project_source import fingerprint

    case = CONTRACT['cases'][1]
    doc, _ = scene(case)
    pixels = doc.image.numpy()
    height, width = pixels.shape[:2]
    header = f'P6\n{width} {height}\n255\n'.encode()
    slide = tmp_path / 'slide.ppm'
    slide.write_bytes(header + pixels.tobytes())
    project = q.Project()
    image = project.add_image(str(slide), 'ppm')
    project.apply_source_review(project.review_source_verification(image, fingerprint(slide)))
    source = tmp_path / 'verified.qpx'
    project.save(str(source))
    del project
    original = source.read_bytes()
    before = tmp_path / 'before.qptma'
    result = cli(*arguments(source, before, image))
    assert result.returncode == 0 and json.loads(result.stdout)['candidates'] == 11, result

    replacement = tmp_path / 'replacement.ppm'
    shifted = np.pad(pixels[:, :-3], ((0, 0), (3, 0), (0, 0)), constant_values=255)
    replacement.write_bytes(header + shifted.tobytes())
    assert replacement.stat().st_size == slide.stat().st_size
    replacement.replace(slide)
    denied = tmp_path / 'denied.qptma'
    result = cli(*arguments(source, denied, image))
    assert result.returncode == 1 and 'differs from saved source verification' in result.stderr, result
    assert not denied.exists()

    project = q.Project.open(str(source))
    review = project.review_source_verification(image, fingerprint(slide))
    assert review.comparison == 'changed_bytes' and review.requires_acceptance
    project.apply_source_review(review, accept_changed=True)
    adopted = tmp_path / 'adopted.qpx'
    project.save(str(adopted))
    proposal = tmp_path / 'shifted.qptma'
    result = cli(*arguments(adopted, proposal, image))
    assert result.returncode == 0 and json.loads(result.stdout)['candidates'] == 11, result
    grid = q.load_qptma(str(proposal))
    for core, expected in zip(grid.cores, case['cores'], strict=True):
        x, y = expected['center']
        assert core.missing != expected['present']
        assert core.bounds.x + core.bounds.width / 2 == pytest.approx(x + 3, abs=2)
        assert core.bounds.y + core.bounds.height / 2 == pytest.approx(y, abs=2)
    assert source.read_bytes() == original
    no_temporaries(tmp_path)


def test_cli_grid_workflow_protects_unselected_label_stores(tmp_path):
    source, image = source_project(tmp_path)
    project = q.Project.open(str(source))
    online, offline = tmp_path / 'labels.zarr', tmp_path / 'offline.zarr'
    online.mkdir()
    sentinel = online / 'source-marker'
    sentinel.write_bytes(b'label store fixture')
    records = {
        str(index): {'ngff_source': True, 'parameters': json.dumps({'method': 3, 'path': str(path)})}
        for index, path in enumerate((online, offline), 1)
    }
    project.document(1).set_image_data_properties(q.ImageDataProperties({'qpx.label-analysis/1': records}))
    source = tmp_path / 'with-labels.qpxproj'
    project.save_new(str(source))
    original = source.read_bytes()

    def protected(args, target):
        result = cli(*args)
        assert result.returncode == 1
        assert 'outside the protected input directory' in json.loads(result.stderr)['error']
        assert not target.exists()

    for destination in (online / 'proposal.qptma', offline):
        protected(arguments(source, destination, image), destination)
    proposal = tmp_path / 'proposal.qptma'
    result = cli(*arguments(source, proposal, image))
    assert result.returncode == 0, result.stderr
    proposal_bytes = proposal.read_bytes()
    for destination in (online / 'applied.qpxproj', offline):
        protected(['--import-qptma', source, proposal, destination, '--image', image], destination)
    applied = tmp_path / 'applied.qpxproj'
    result = cli('--import-qptma', source, proposal, applied, '--image', image)
    assert result.returncode == 0, result.stderr
    assert json.loads(result.stdout)['cores'] == 12
    applied_bytes = applied.read_bytes()
    for destination in (online / 'exported.qptma', offline):
        protected(['--export-qptma', applied, destination, '--image', image], destination)
    exported = tmp_path / 'exported.qptma'
    result = cli('--export-qptma', applied, exported, '--image', image)
    assert result.returncode == 0, result.stderr
    cores = q.load_qptma(str(exported)).cores
    assert len(cores) == 12 and cores[5].missing
    assert (source.read_bytes(), proposal.read_bytes(), applied.read_bytes()) == (original, proposal_bytes, applied_bytes)
    assert {path.name: path.read_bytes() for path in online.iterdir()} == {sentinel.name: b'label store fixture'}
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
