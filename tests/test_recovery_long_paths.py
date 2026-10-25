"""Private recovery paths can exceed MAX_PATH without changing logical paths."""
import os
from pathlib import Path
import shutil

import pytest
import qupathxx as q

pytestmark = pytest.mark.skipif(not q.has_project, reason='project storage required')


def access(path):
    text = str(path.resolve())
    if os.name == 'nt' and not text.startswith('\\\\?\\'):
        return Path('\\\\?\\UNC\\' + text[2:] if text.startswith('\\\\') else '\\\\?\\' + text)
    return Path(text)


def target_path(tmp_path):
    directory = tmp_path / 'nested'
    directory.mkdir()
    suffix = '.qpxrecover'
    leaf_length = 230 - len(str(directory.resolve())) - 1 - len(suffix)
    assert 1 <= leaf_length < 240, 'test root must leave room for the public checkpoint name'
    target = directory / ('r' * leaf_length + suffix)
    assert len(str(target.resolve())) == 230
    assert len(str(target.resolve()) + '.work-' + 'c' * 32) > 260
    return target


def scene(tmp_path):
    project = q.Project()
    image = project.add_document(q.Document(q.Image(8, 8, [42] * 8 * 8 * 3)))
    obj = project.document(image).add_rectangle(q.Rect(1, 1, 3, 3), 'Captured')
    primary = tmp_path / 'primary.qpxproj'
    project.save(str(primary))
    target = target_path(tmp_path)
    writer = q.ProjectAutosave()
    assert writer.request(project, str(target))
    result = writer.wait()
    assert result.state == 'ready', result.error
    assert result.path == str(target.resolve())
    return project, image, obj, primary, target


def stage(target, info, valid_snapshot=True):
    work = access(Path(str(target) + '.work-' + 'c' * 32))
    work.mkdir()
    (work / 'owner').write_bytes(('qupathxx recovery work v2\n' + 'c' * 32 + '\n' + info.identity + '\n' + info.session + '\n' + info.base_path).encode('utf-8'))
    if valid_snapshot:
        shutil.copyfile(access(target), work / 'snapshot.qpxproj')
    return work


def test_public_checkpoint_survives_long_private_work_and_snapshot_paths(tmp_path):
    project, image, obj, primary, target = scene(tmp_path)
    primary_bytes = primary.read_bytes()
    info = q.Project.inspect_recovery(str(target))
    assert info.path == str(target.resolve()) and info.base_path == str(primary.resolve())
    assert q.Project.recover(str(target)).document(image).annotation(obj).name == 'Captured'
    assert str(target.resolve()) in [i.path for i in q.Project.find_recoveries(str(primary))]
    assert primary.read_bytes() == primary_bytes
    assert not [p for p in access(target.parent).iterdir() if '.work-' in p.name]


def test_long_abandoned_work_rescues_committed_snapshot_without_overwrite(tmp_path):
    project, image, obj, primary, target = scene(tmp_path)
    info = q.Project.inspect_recovery(str(target))
    work = stage(target, info)
    before = access(target).read_bytes()
    rescue = target.parent / (target.name + '.rescued-' + 'c' * 32 + '.qpxrecover')
    assert q.Project.cleanup_recovery_work(str(target)) == 1
    assert not work.exists() and access(target).read_bytes() == before
    assert q.Project.recover(str(rescue)).document(image).annotation(obj).name == 'Captured'
    assert str(rescue.resolve()) in [i.path for i in q.Project.find_recoveries(str(primary))]


@pytest.mark.parametrize('kind', ['collision', 'extra_file', 'wrong_owner'])
def test_long_work_preserves_collisions_and_unrecognized_inventory(tmp_path, kind):
    project, image, obj, primary, target = scene(tmp_path)
    info = q.Project.inspect_recovery(str(target))
    work = stage(target, info)
    rescue = access(target.parent / (target.name + '.rescued-' + 'c' * 32 + '.qpxrecover'))
    if kind == 'collision':
        rescue.write_bytes(b'preserve existing rescue')
    elif kind == 'extra_file':
        (work / 'private.txt').write_bytes(b'preserve unknown data')
    else:
        (work / 'owner').write_bytes(b'unrecognized owner')
    before = {p.name: p.read_bytes() for p in work.iterdir()}
    checkpoint = access(target).read_bytes()
    if kind == 'collision':
        with pytest.raises(RuntimeError, match='without replacing'):
            q.Project.cleanup_recovery_work(str(target))
        assert rescue.read_bytes() == b'preserve existing rescue'
    else:
        assert q.Project.cleanup_recovery_work(str(target)) == 0
    assert {p.name: p.read_bytes() for p in work.iterdir()} == before
    assert access(target).read_bytes() == checkpoint
