"""Independent SVD/quantiles, known absorbers, sample bounds and shared GUI use."""
import gc
import hashlib
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

import numpy as np
import pytest
import qupathxx as q

WHITE=np.array([255.,250.,245.])
TRUE=np.array([[.65,.70,.29],[.07,.99,.11]])
TRUE/=np.linalg.norm(TRUE,axis=1)[:,None]


def phantom(size=128):
    y,x=np.indices((size,size));a=np.where(x%8==0,0,.1+.6*x/(size-1));b=np.where(x%8==1,0,.1+.7*y/(size-1))
    od=a[...,None]*TRUE[0]+b[...,None]*TRUE[1]
    return np.rint(WHITE*10**(-od)).clip(0,255).astype(np.uint8)


def setup(size=128,parameters=None,roi=None,nucleus=False):
    rgb=phantom(size);doc=q.Document(q.Image(size,size,rgb.ravel().tolist()))
    shape=roi or q.Roi.rectangle(q.Rect(0,0,size,size))
    obj=doc.add_cell(shape,q.Roi.rectangle(q.Rect(size/4,size/4,size/2,size/2))) if nucleus else doc.add_roi(shape)
    request=q.StainEstimateRequest(doc,obj,parameters or q.StainEstimateParameters(),WHITE.tolist(),nucleus)
    return doc,obj,request,rgb


def retained(rgb,p,mask=None):
    """Tile traversal + independently written integer rejection reservoir."""
    keep=[];accepted=0;state=p.seed;mask=np.ones(rgb.shape[:2],bool) if mask is None else mask
    bits=(1<<64)-1
    for y in range(0,rgb.shape[0],p.tile_size):
        for x in range(0,rgb.shape[1],p.tile_size):
            block=rgb[y:y+p.tile_size,x:x+p.tile_size]
            od=np.maximum(0,-np.log10(np.maximum(1,block.astype(float))/WHITE))
            domain=mask[y:y+p.tile_size,x:x+p.tile_size] & np.all(od>=p.min_od,axis=2) & np.any(od!=0,axis=2)
            for point in od[domain]:
                accepted+=1
                if len(keep)<p.sample_limit:keep.append(point)
                else:
                    threshold=(1<<64)%accepted
                    while True:
                        state=(state+0x9e3779b97f4a7c15)&bits;z=state
                        z=((z^(z>>30))*0xbf58476d1ce4e5b9)&bits;z=((z^(z>>27))*0x94d049bb133111eb)&bits;z^=z>>31
                        if z>=threshold:break
                    index=z%accepted
                    if index<len(keep):keep[index]=point
    return np.array(keep),accepted


def reference(points,p):
    # SVD of the full sampled matrix independently validates the native 3x3
    # Jacobi eigensolver of its uncentered second moment.
    _,singular,axes=np.linalg.svd(points,full_matrices=False)
    normal=np.cross(axes[0],axes[1]);normal/=np.linalg.norm(normal)
    if normal[np.argmax(np.abs(normal))]<0:normal=-normal
    first=axes[:2].T@(axes[:2]@points.mean(axis=0));first/=np.linalg.norm(first)
    second=np.cross(normal,first);second/=np.linalg.norm(second)
    angles=np.arctan2(points@second,points@first)
    low,high=np.quantile(angles,[p.trim_quantile,1-p.trim_quantile])
    stains=np.array([first*np.cos(v)+second*np.sin(v) for v in [low,high]])
    stains=np.maximum(stains,0);stains/=np.linalg.norm(stains,axis=1)[:,None]
    if tuple(stains[0])<tuple(stains[1]):stains=stains[::-1]
    residual=np.cross(stains[0],stains[1]);residual/=np.linalg.norm(residual)
    return np.vstack([stains,residual]),singular**2/len(points)


@pytest.mark.parametrize('limit,seed',[(200000,0),(257,0),(257,42),(64,2**64-1)])
def test_matches_independent_svd_quantile_and_reservoir_reference(limit,seed):
    p=q.StainEstimateParameters();p.sample_limit=limit;p.seed=seed;p.tile_size=32
    doc,obj,request,rgb=setup(parameters=p);actual=q.estimate_stains(request);points,accepted=retained(rgb,p);expected,spectrum=reference(points,p)
    np.testing.assert_allclose(actual.stains,expected,rtol=0,atol=2e-11)
    np.testing.assert_allclose(actual.eigenvalues,spectrum,rtol=2e-11,atol=2e-14)
    assert actual.eligible==accepted and actual.sampled==min(accepted,limit) and actual.samples==128**2 and actual.tiles==16
    assert actual.residual_fraction==pytest.approx(spectrum[-1]/spectrum.sum(),abs=2e-14)
    assert actual.provenance is request.provenance and actual.provenance.object_id==obj
    assert q.stain_model_sha256(actual.transform)==actual.model_sha256


def test_known_stains_are_recovered_and_input_digest_matches_independent_rgba_tiles():
    p=q.StainEstimateParameters();p.tile_size=32;doc,obj,request,rgb=setup(parameters=p);result=q.estimate_stains(request)
    angles=np.degrees(np.arccos(np.clip(np.sum(np.array(result.stains[:2])*TRUE,axis=1),-1,1)))
    assert np.max(angles)<.5 and result.angle_degrees>20
    digest=hashlib.sha256(b'qpx.rgba-tiles/1');digest.update(struct.pack('<7Q',128,128,0,0,128,128,32))
    for y in range(0,128,32):
        for x in range(0,128,32):
            digest.update(struct.pack('<4Q',x,y,32,32));rgba=np.full((32,32,4),255,np.uint8);rgba[:,:,:3]=rgb[y:y+32,x:x+32];digest.update(rgba.tobytes())
    assert result.input_sha256==digest.hexdigest()


@pytest.mark.skipif(not q.has_project or not q.has_interchange, reason='requires project and model-file support')
def test_batch_estimate_saved_roi_and_reuse_exact_model(tmp_path):
    from qupathxx import _native

    bundled = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    batch = os.environ.get('QUPATHXX_TEST_STAIN_BATCH', str(bundled))
    doc, roi, _, rgb = setup(64)
    project = q.Project()
    image = project.add_document(doc)
    linked = tmp_path / 'linked.ppm'
    linked.write_bytes(b'P6\n64 64\n255\n' + rgb.tobytes())
    project.add_image(str(linked), reader='ppm')
    source = tmp_path / 'source.qpxproj'
    project.save(str(source))
    before = source.read_bytes(), linked.read_bytes()

    p = q.StainEstimateParameters()
    p.level = 0
    p.seed = 2**64 - 1
    p.tile_size = 16
    p.sample_limit = 257
    opened = q.Project.open(str(source)).document(image)
    expected = q.estimate_stains(q.StainEstimateRequest(opened, roi, p, WHITE.tolist()))

    def run(*args):
        return subprocess.run([batch, *map(str, args)], capture_output=True, text=True, timeout=30)

    def fit(path, *extra):
        return run('--estimate-stains', source, path, '--image', image, '--roi', roi,
                   '--white', '255,250,245', '--level', 0, '--seed', p.seed,
                   '--tile-size', p.tile_size, '--sample-limit', p.sample_limit, *extra)

    model_path = tmp_path / 'estimate.json'
    for path in [model_path, tmp_path / 'repeat.json']:
        result = fit(path)
        assert result.returncode == 0, result.stderr
        report = json.loads(path.read_bytes())
        assert json.loads(result.stdout) == report
        assert report['provenance']['object_id'] == roi
        assert report['input_sha256'] == expected.input_sha256
        assert report['model_sha256'] == expected.model_sha256
        assert report['sampled'] == p.sample_limit and report['parameters']['seed'] == p.seed
        np.testing.assert_array_equal(report['stains'], expected.stains)
        assert q.stain_model_sha256(q.load_stain_model(str(path))) == expected.model_sha256

    existing = model_path.read_bytes()
    wal = Path(str(source) + '-wal')
    protected = [model_path, source, linked, wal]
    if os.name == 'nt':
        protected.append(wal.with_name(wal.name.upper()))
    for path in protected:
        rejected = fit(path)
        assert rejected.returncode == 2 and rejected.stderr
    assert not wal.exists()
    rejected_path = tmp_path / 'refused.json'
    for args in [('--roi', roi), ('--work-bytes', 1), ('--trim-quantile', 0.5)]:
        rejected = fit(rejected_path, *args)
        assert rejected.returncode == 2 and not rejected_path.exists()
    bad_roi = run('--estimate-stains', source, rejected_path, '--image', image, '--roi', roi + 100)
    assert bad_roi.returncode == 2 and not rejected_path.exists()
    assert model_path.read_bytes() == existing
    assert (source.read_bytes(), linked.read_bytes()) == before
    assert not list(tmp_path.glob('*.qpx-tmp-*'))

    args = (source, tmp_path / 'stain batch', '--kind', 'stains', '--image', image,
            '--object', roi, '--stain-model', model_path)
    result = run(*args)
    assert result.returncode == 0, result.stderr
    report = json.loads(result.stdout.splitlines()[-1])
    assert report['saved'] == 1 and report['images'][0]['records'] == 1
    assert Path(report['images'][0]['path']).is_file()
    vectors = ','.join(str(component) for row in expected.stains for component in row)
    repeated = run(*args[:-2], '--white', '255,250,245', '--stains', vectors, '--normalized')
    assert repeated.returncode == 0 and json.loads(repeated.stdout.splitlines()[-1])['reused'] == 1

    saved_project = tmp_path / 'with-stain-model.qpx'
    apply = ('--apply-stain-model', source, saved_project, '--image', image,
             '--stain-model', model_path, '--profile-name', 'Approved ROI model')
    applied = run(*apply)
    assert applied.returncode == 0, applied.stderr
    assert json.loads(applied.stdout)['model_sha256'] == expected.model_sha256
    from contextlib import closing
    import sqlite3

    def text(value):
        raw = value.encode('utf-8')
        return struct.pack('<Q', len(raw)) + raw

    labels = {'schema': 'qpx.image-analysis-settings/1', 'image_type': 'BRIGHTFIELD_OTHER',
              'name': 'Approved ROI model', 'residual': '0',
              **{f'stain_{i}': f'Stain {i + 1}' for i in range(3)}}
    numbers = {**{f'white_{i}': float(expected.white[i]) for i in range(3)},
               **{f'basis_{i}_{j}': float(expected.stains[i][j]) for i in range(3) for j in range(3)}}
    payload = b'QOP\x01' + struct.pack('<Q', len(labels))
    payload += b''.join(text(key) + text(value) for key, value in sorted(labels.items()))
    payload += struct.pack('<Q', len(numbers))
    payload += b''.join(text(key) + struct.pack('<d', value) for key, value in sorted(numbers.items()))
    payload += hashlib.sha256(payload).hexdigest().encode()
    with closing(sqlite3.connect(saved_project)) as db:
        assert db.execute('SELECT image_id,payload FROM image_analysis_settings').fetchall() == [(str(image), payload)]
    applied_before = saved_project.read_bytes()
    assert run(*apply).returncode == 2 and saved_project.read_bytes() == applied_before

    saved_batch = run(saved_project, tmp_path / 'saved model results', '--kind', 'stains', '--image', image,
                      '--object', roi, '--saved-stains')
    assert saved_batch.returncode == 0, saved_batch.stderr
    output = json.loads(saved_batch.stdout.splitlines()[-1])
    assert output['saved'] == 1 and output['images'][0]['pixel_sha256'] == report['images'][0]['pixel_sha256']
    from csv import DictReader

    def means(batch_report, filename):
        export = tmp_path / filename
        result = run('--export-csv', batch_report['images'][0]['path'], export)
        assert result.returncode == 0, result.stderr
        row, = DictReader(io.StringIO(export.read_text(encoding='utf-8')))
        return [float(row[f'mean_channel_{i}']) for i in (1, 2, 3)]

    assert means(output, 'saved-means.csv') == means(report, 'explicit-means.csv')
    for conflict in (source, model_path, linked, wal):
        rejected = run(*apply[:2], conflict, *apply[3:])
        assert rejected.returncode == 2
    corrupt = tmp_path / 'corrupt-model.json'
    modified = json.loads(existing)
    modified['model_sha256'] = 'f' * 64
    corrupt.write_text(json.dumps(modified), encoding='utf-8')
    bad = run('--apply-stain-model', source, rejected_path, '--image', image, '--stain-model', corrupt)
    assert bad.returncode == 2 and 'digest mismatch' in bad.stderr and not rejected_path.exists()
    assert not wal.exists() and (source.read_bytes(), linked.read_bytes(), model_path.read_bytes()) == (*before, existing)

    project.document(image + 1).set_image_analysis_settings(q.ImageAnalysisSettings('FLUORESCENCE'))
    fluorescence = tmp_path / 'fluorescence.qpx'
    project.save(str(fluorescence))
    unsupported = run('--apply-stain-model', fluorescence, rejected_path, '--image', image + 1,
                      '--stain-model', model_path)
    assert unsupported.returncode == 2 and 'require a brightfield image type' in unsupported.stderr
    assert not rejected_path.exists()


@pytest.mark.skipif(not q.has_project, reason='requires project storage')
def test_export_stain_model_from_saved_project_with_offline_image(tmp_path):
    from qupathxx import _native

    bundled = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    batch = os.environ.get('QUPATHXX_TEST_STAIN_BATCH', str(bundled))
    source = tmp_path / 'linked.ppm'
    source.write_bytes(b'P6\n2 2\n255\n' + bytes(range(12)))
    project = q.Project()
    image = project.add_image(str(source), reader='ppm')
    bare = project.add_document(q.Document(q.Image.demo()))
    settings = q.ImageAnalysisSettings('BRIGHTFIELD_OTHER', [[.6, .8, 0], [.8, -.6, 0], [0, 0, -1]],
                                       [250, 245, 230], 'Portable', ['A', 'B', 'C'])
    project.document(image).set_image_analysis_settings(settings)
    path = tmp_path / 'source.qpx'
    project.save(str(path))
    original = path.read_bytes()
    archived_path = tmp_path / 'source-offline.ppm'
    project.document(image).server.close()
    source.rename(archived_path)

    def run(*args):
        return subprocess.run([batch, *map(str, args)], capture_output=True, encoding='utf-8', timeout=30)

    target = tmp_path / 'model.json'
    args = ('--export-stain-model', path, target, '--image', image)
    result = run(*args)
    assert result.returncode == 0, result.stderr
    assert json.loads(result.stdout)['model_sha256'] == settings.model_sha256
    assert target.read_bytes() == q.stain_model_json(settings.transform()).encode()
    assert run(*args).returncode == 2 and target.read_bytes() == q.stain_model_json(settings.transform()).encode()
    denied = tmp_path / 'not-written.json'
    result = run('--export-stain-model', path, denied, '--image', bare)
    assert result.returncode == 2 and 'saved RGB8 stains' in result.stderr and not denied.exists()
    for protected in (path, Path(str(path) + '-wal'), source):
        refused = run('--export-stain-model', path, protected, '--image', image)
        assert refused.returncode == 2 and 'input' in refused.stderr
    assert path.read_bytes() == original and not source.exists() and not Path(str(path) + '-wal').exists()
    assert archived_path.read_bytes() == b'P6\n2 2\n255\n' + bytes(range(12))

    other_project = q.Project()
    other_image = other_project.add_document(q.Document(q.Image.demo()))
    other = tmp_path / 'other.qpx'
    other_project.save(str(other))
    applied_path = tmp_path / 'applied.qpx'
    applied = run('--apply-stain-model', other, applied_path, '--image', other_image, '--stain-model', target)
    assert applied.returncode == 0, applied.stderr
    second = tmp_path / 'round-trip.json'
    copied = run('--export-stain-model', applied_path, second, '--image', other_image)
    assert copied.returncode == 0, copied.stderr
    assert second.read_bytes() == target.read_bytes()


@pytest.mark.skipif(not q.has_project, reason='requires project storage')
def test_batch_uses_each_images_saved_stain_defaults(tmp_path):
    import csv
    import io
    from qupathxx import _native

    bundled = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    batch = os.environ.get('QUPATHXX_TEST_STAIN_BATCH', str(bundled))
    project = q.Project()
    images = []
    for white in ([255, 245, 235], [200, 210, 220]):
        doc, roi, _, _ = setup(32)
        settings = q.ImageAnalysisSettings('BRIGHTFIELD_OTHER', [[1, 0, 0], [0, 1, 0], [0, 0, 1]],
                                           white, 'Saved RGB', ['R', 'G', 'B'])
        doc.set_image_analysis_settings(settings)
        images.append((project.add_document(doc), roi))
    source = tmp_path / 'saved.qpx'
    project.save(str(source))
    original = source.read_bytes()
    output = tmp_path / 'batch'

    def run(*args):
        return subprocess.run([batch, *map(str, args)], capture_output=True, encoding='utf-8', timeout=30)

    command = (source, output, '--kind', 'stains', '--saved-stains')
    result = run(*command)
    assert result.returncode == 0, result.stderr
    report = json.loads(result.stdout.splitlines()[-1])
    assert report['saved'] == 2 and report['reused'] == 0
    means = []
    saved = q.Project.open(str(source))
    for item, (image, roi) in zip(report['images'], images, strict=True):
        assert item['image_id'] == image
        doc = saved.document(image)
        expected = q.analyze(q.AnalysisRequest(doc, roi, doc.image_analysis_settings.transform(),
                                               kind=q.AnalysisKind.STAIN_MEANS))
        exported = tmp_path / f'measurements-{image}.csv'
        result = run('--export-csv', item['path'], exported)
        assert result.returncode == 0, result.stderr
        row, = csv.DictReader(io.StringIO(exported.read_text(encoding='utf-8')))
        means.append([float(row[f'mean_channel_{n}']) for n in (1, 2, 3)])
        assert means[-1] == expected.means and row['input_sha256'] == expected.input_sha256
    assert means[0] != means[1]
    repeated = run(*command)
    assert repeated.returncode == 0 and json.loads(repeated.stdout.splitlines()[-1])['reused'] == 2
    assert source.read_bytes() == original

    for kind in ('tissue', 'cells'):
        image, roi = images[1]
        common = ('--kind', kind, '--image', image, '--object', roi)
        from_saved = run(source, tmp_path / f'{kind}-saved', *common, '--saved-stains')
        explicit = run(source, tmp_path / f'{kind}-explicit', *common, '--white', '200,210,220',
                       '--stains', '1,0,0,0,1,0,0,0,1', '--normalized')
        assert from_saved.returncode == explicit.returncode == 0, (from_saved.stderr, explicit.stderr)
        tables = []
        for mode, result in [('saved', from_saved), ('explicit', explicit)]:
            [image_result] = json.loads(result.stdout.splitlines()[-1])['images']
            csv_path = tmp_path / f'{kind}-{mode}.csv'
            exported = run('--export-csv', image_result['path'], csv_path)
            assert exported.returncode == 0, exported.stderr
            tables.append([{key: value for key, value in row.items() if key not in ('captured_unix_ms', 'elapsed_ms')}
                           for row in csv.DictReader(io.StringIO(csv_path.read_text(encoding='utf-8')))])
        assert tables[0] and tables[0] == tables[1]

    for args in [('--white', '255,255,255'), ('--stain-model', 'other.json')]:
        conflict = run(*command, *args)
        assert conflict.returncode == 2 and 'conflicts' in conflict.stderr
    incompatible = run(source, tmp_path / 'raw', '--kind', 'raw', '--saved-stains')
    assert incompatible.returncode == 2 and not (tmp_path / 'raw').exists()
    changed_image = saved.document(images[1][0])
    changed_image.set_image_analysis_settings(q.ImageAnalysisSettings(
        'BRIGHTFIELD_OTHER', [[1, 0, 0], [0, 1, 0], [0, 0, 1]], [190, 205, 215], 'Updated RGB', ['R', 'G', 'B']))
    saved.save(str(source))
    changed = run(*command)
    assert changed.returncode == 0, changed.stderr
    updated = json.loads(changed.stdout.splitlines()[-1])['images'][1]
    assert updated['state'] == 'saved' and updated['input_sha256'] != report['images'][1]['input_sha256']
    assert updated['pixel_sha256'] == report['images'][1]['pixel_sha256']
    updated_csv = tmp_path / 'updated-measurements.csv'
    result = run('--export-csv', updated['path'], updated_csv)
    assert result.returncode == 0, result.stderr
    row, = csv.DictReader(io.StringIO(updated_csv.read_text(encoding='utf-8')))
    updated_means = [float(row[f'mean_channel_{n}']) for n in (1, 2, 3)]
    expected = q.analyze(q.AnalysisRequest(changed_image, images[1][1], changed_image.image_analysis_settings.transform(),
                                           kind=q.AnalysisKind.STAIN_MEANS))
    assert updated_means == expected.means and updated_means != means[1]

    saved.document(images[1][0]).set_image_analysis_settings()
    missing_source = tmp_path / 'missing-model.qpx'
    saved.save(str(missing_source))
    missing_output = tmp_path / 'missing-model-results'
    missing = run(missing_source, missing_output, '--kind', 'stains', '--saved-stains')
    assert missing.returncode == 2 and 'saved RGB8 stains' in missing.stderr and not missing_output.exists()


@pytest.mark.skipif(not q.has_project, reason='requires project storage')
def test_batch_estimate_requires_review_for_replaced_verified_image(tmp_path):
    from qupathxx import _native
    from test_project_source import fingerprint

    bundled = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    batch = os.environ.get('QUPATHXX_TEST_STAIN_BATCH', str(bundled))
    pixels = phantom(64)
    header = b'P6\n64 64\n255\n'
    slide = tmp_path / 'source.ppm'
    slide.write_bytes(header + pixels.tobytes())
    project = q.Project()
    image = project.add_image(str(slide), 'ppm')
    roi = project.document(image).add_rectangle(q.Rect(0, 0, 64, 64))
    project.apply_source_review(project.review_source_verification(image, fingerprint(slide)))
    source = tmp_path / 'verified.qpx'
    project.save(str(source))
    del project
    original = source.read_bytes()

    def estimate(project_path, output):
        return subprocess.run([batch, '--estimate-stains', str(project_path), str(output),
                               '--image', str(image), '--roi', str(roi), '--white', '255,250,245',
                               '--level', '0', '--tile-size', '16', '--sample-limit', '257'],
                              capture_output=True, text=True, timeout=30)

    initial = tmp_path / 'initial.json'
    result = estimate(source, initial)
    assert result.returncode == 0, result.stderr
    old_digest = json.loads(initial.read_bytes())['input_sha256']
    replacement = tmp_path / 'replacement.ppm'
    replacement.write_bytes(header + np.roll(pixels, 3, axis=1).tobytes())
    replacement.replace(slide)
    denied = tmp_path / 'denied.json'
    result = estimate(source, denied)
    assert result.returncode == 2 and 'differs from saved source verification' in result.stderr, result
    assert not denied.exists() and not list(tmp_path.glob('*.qpx-tmp-*'))

    project = q.Project.open(str(source))
    review = project.review_source_verification(image, fingerprint(slide))
    assert review.comparison == 'changed_bytes' and review.requires_acceptance
    project.apply_source_review(review, accept_changed=True)
    adopted = tmp_path / 'adopted.qpx'
    project.save(str(adopted))
    model = tmp_path / 'adopted.json'
    result = estimate(adopted, model)
    assert result.returncode == 0, result.stderr
    report = json.loads(model.read_bytes())
    parameters = q.StainEstimateParameters()
    parameters.level, parameters.tile_size, parameters.sample_limit = 0, 16, 257
    expected = q.estimate_stains(q.StainEstimateRequest(project.document(image), roi, parameters, WHITE.tolist()))
    assert report['input_sha256'] != old_digest
    assert report['input_sha256'] == expected.input_sha256 and report['model_sha256'] == expected.model_sha256
    np.testing.assert_array_equal(report['stains'], expected.stains)
    assert source.read_bytes() == original


def test_model_identity_json_atomic_export_and_exact_transform_reload(tmp_path):
    doc,obj,request,_=setup();result=q.estimate_stains(request);text=q.stain_estimate_json(result);data=json.loads(text)
    assert data['schema']=='qpx.stain-estimate/1' and data['algorithm']==result.algorithm and data['model_sha256']==result.model_sha256
    model=q.StainTransform.from_normalized(data['stains'],data['white']);assert q.stain_model_sha256(model)==result.model_sha256
    digest=hashlib.sha256(b'qpx.stain-model/1')
    for value in [*data['white'],*(v for stain in data['stains'] for v in stain)]:digest.update(struct.pack('<d',0. if value==0 else value))
    assert digest.hexdigest()==result.model_sha256
    path=tmp_path/'\u8272.json';q.save_stain_estimate(result,str(path));assert path.read_bytes()==text.encode()
    directory=tmp_path/'occupied';directory.mkdir()
    with pytest.raises(RuntimeError):q.save_stain_estimate(result,str(directory))
    assert directory.is_dir() and not list(tmp_path.glob('*.qpx-tmp-*'))
    means=q.analyze(q.AnalysisRequest(doc,obj,model));same=q.analyze(q.AnalysisRequest(doc,obj,result.transform))
    np.testing.assert_array_equal(means.means,same.means)
    del doc,request;gc.collect();assert q.stain_estimate_json(result)==text
    with pytest.raises(AttributeError):result.model_sha256='changed'
    params=result.parameters;params.seed=999;assert result.parameters.seed==0


@pytest.mark.parametrize('field,value',[('min_od',-1),('min_od',float('nan')),('trim_quantile',.3),('min_angle_degrees',0),('max_residual_fraction',.3),('min_samples',7),('sample_limit',63),('sample_limit',1000001),('tile_size',15),('work_bytes',0),('max_read_pixels',0),('level',-2)])
def test_invalid_parameters_rejected(field,value):
    p=q.StainEstimateParameters();setattr(p,field,value)
    with pytest.raises(ValueError):p.validate()


def test_exact_work_budget_and_read_limits_are_enforced_before_worker():
    doc,obj,request,_=setup();p=request.parameters;p.work_bytes=request.estimated_work_bytes
    assert q.StainEstimateRequest(doc,obj,p,WHITE.tolist()).estimated_work_bytes==p.work_bytes
    p.work_bytes-=1
    with pytest.raises(ValueError,match='limits'):q.StainEstimateRequest(doc,obj,p,WHITE.tolist())
    p=q.StainEstimateParameters();p.max_read_pixels=100
    with pytest.raises(ValueError,match='limits'):q.StainEstimateRequest(doc,obj,p,WHITE.tolist())
    p.level=99
    with pytest.raises(IndexError):q.StainEstimateRequest(doc,obj,p)


@pytest.mark.parametrize('rgb_value',[0,80,255])
def test_constant_or_blank_regions_fail_without_synthetic_vectors(rgb_value):
    doc=q.Document(q.Image(32,32,[rgb_value]*32*32*3));obj=doc.add_rectangle(q.Rect(0,0,32,32))
    with pytest.raises(ValueError):q.estimate_stains(q.StainEstimateRequest(doc,obj))
    job=q.StainEstimateJob();assert job.request(q.StainEstimateRequest(doc,obj));status=job.wait(doc,obj)
    assert status.state=='failed' and status.error and job.result is None


def test_nucleus_and_ellipse_use_selected_roi_pixel_centers():
    doc,obj,request,_=setup(nucleus=True);result=q.estimate_stains(request)
    assert result.provenance.nucleus and result.samples==64**2 and result.roi.bounds.x==32
    y,x=np.indices((128,128));mask=((x+.5-64)/50)**2+((y+.5-64)/45)**2<=1
    roi=q.Roi.ellipse(q.Rect(14,19,100,90));doc,obj,request,rgb=setup(roi=roi);result=q.estimate_stains(request)
    assert result.samples==int(mask.sum()) and result.roi.kind==q.RoiKind.ELLIPSE


def test_jobs_reject_stale_selection_source_and_owner_identity_and_can_cancel():
    doc,obj,request,_=setup();job=q.StainEstimateJob();assert job.request(request);assert job.wait(doc,obj).state=='ready'
    expected=job.result.model_sha256;doc.rename_annotation(obj,'changed');assert job.poll(doc,obj).state=='stale' and job.result is None
    fresh=q.StainEstimateRequest(doc,obj,white=WHITE.tolist());assert job.request(fresh);job.cancel();assert job.wait(doc,obj).state=='cancelled' and job.result is None
    assert job.request(fresh);assert job.wait(doc,obj).state=='ready' and job.result.model_sha256==expected
    doc.server.close();assert job.poll(doc,obj).state=='stale' and job.result is None


@pytest.mark.skipif(not q.has_openslide,reason='native pyramid')
def test_auto_level_bounds_read_work_and_uses_real_coarse_pixels(tmp_path):
    tifffile=pytest.importorskip('tifffile');path=tmp_path/'stains.tif'
    with tifffile.TiffWriter(path) as writer:
        for i,size in enumerate([512,128]):writer.write(phantom(size),tile=(64,64),photometric='rgb',metadata=None,subfiletype=0 if i==0 else 1)
    server=q.open_slide(str(path));doc=q.Document(server);obj=doc.add_rectangle(q.Rect(0,0,512,512));p=q.StainEstimateParameters();p.max_read_pixels=20000
    request=q.StainEstimateRequest(doc,obj,p,WHITE.tolist());result=q.estimate_stains(request)
    assert request.level==result.level==1 and result.downsample==4 and result.samples==128**2
    points,accepted=retained(phantom(128),p);expected,_=reference(points,p)
    np.testing.assert_allclose(result.stains,expected,rtol=0,atol=2e-11)
    digest=hashlib.sha256(b'qpx.rgba-level-tiles/1');digest.update(struct.pack('<5Qd5Q',512,512,1,128,128,4.,0,0,128,128,256));digest.update(struct.pack('<6Q',0,0,0,0,128,128))
    rgba=np.full((128,128,4),255,np.uint8);rgba[:,:,:3]=phantom(128);digest.update(rgba.tobytes());assert result.input_sha256==digest.hexdigest()


@pytest.mark.skipif(not q.has_project,reason='native PPM factory')
def test_model_export_preserves_its_source_file_and_alias(tmp_path):
    rgb=phantom();path=tmp_path/'source.ppm';path.write_bytes(b'P6\n128 128\n255\n'+rgb.tobytes());project=q.Project();image=project.add_image(str(path),reader='ppm');doc=project.document(image);obj=doc.add_rectangle(q.Rect(0,0,128,128));result=q.estimate_stains(q.StainEstimateRequest(doc,obj,white=WHITE.tolist()))
    before=path.read_bytes()
    with pytest.raises(ValueError):q.save_stain_estimate(result,str(path))
    alias=tmp_path/'alias.ppm';os.link(path,alias)
    with pytest.raises(ValueError):q.save_stain_estimate(result,str(alias))
    assert path.read_bytes()==before


def test_worker_source_ownership_releases_on_python_owner_thread(tmp_path):
    script=r'''
import gc,sys
import qupathxx as q
rgb=[]
for y in range(64):
 for x in range(64):rgb.extend([50+x,50+y,100+x//2])
doc=q.Document(q.Image(64,64,rgb));obj=doc.add_rectangle(q.Rect(0,0,64,64));p=q.StainEstimateParameters();p.max_residual_fraction=.25
request=q.StainEstimateRequest(doc,obj,p);job=q.StainEstimateJob();assert job.request(request);del request;gc.collect();job.wait(doc,obj);del job,doc;gc.collect()
'''
    child=subprocess.run([sys.executable,'-I','-c',script],capture_output=True,text=True,timeout=20)
    assert child.returncode==0,child.stdout+child.stderr


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='desktop replay')
def test_gui_estimates_applies_and_analyzes_the_exact_native_model():
    doc,obj,request,_=setup();expected=q.estimate_stains(request);options=q.GuiOptions();options.analysis_white=WHITE.tolist()
    def event(frame,kind,wait=q.GuiInputWait.IMMEDIATE):
        e=q.GuiInput();e.frame,e.kind,e.wait_for=frame,kind,wait;return e
    options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(8,q.GuiInputKind.ESTIMATE_STAINS_KEY),event(10,q.GuiInputKind.APPLY_STAIN_ESTIMATE_KEY,q.GuiInputWait.STAINS_ESTIMATED),event(12,q.GuiInputKind.ANALYZE_KEY,q.GuiInputWait.STAINS_APPLIED)]
    report=q.show(doc,frame_limit=40,options=options)
    assert report.stain_estimate_state=='ready' and not report.stain_estimate_error and report.stain_estimate_applied
    assert report.stain_estimate_result.model_sha256==expected.model_sha256 and report.analysis_state=='ready'
    direct=q.analyze(q.AnalysisRequest(doc,obj,expected.transform));np.testing.assert_array_equal(report.analysis_means,direct.means)


def test_estimation_and_job_wait_release_the_python_gil():
    import threading
    doc,obj,request,_=setup(size=1024);calls=[0];stop=threading.Event()
    def count():
        while not stop.is_set():calls[0]+=1
    thread=threading.Thread(target=count);thread.start()
    try:
        before=calls[0];q.estimate_stains(request);assert calls[0]>before
        job=q.StainEstimateJob();assert job.request(request);before=calls[0];assert job.wait(doc,obj).state=='ready' and calls[0]>before
    finally:stop.set();thread.join()


def test_request_identity_and_selected_id_are_reconciled_before_publication():
    doc,obj,request,_=setup();other=q.Document(doc.server);same_id=other.add_rectangle(q.Rect(0,0,128,128));assert same_id==obj
    job=q.StainEstimateJob();assert job.request(request) and not job.request(request)
    assert job.wait(other,same_id).state=='stale' and job.result is None
    assert job.request(request);assert job.wait(doc,0).state=='stale' and job.result is None


@pytest.mark.skipif(not q.has_project,reason='native project')
def test_estimated_model_used_in_analysis_survives_project_restart(tmp_path):
    doc,obj,request,_=setup();model=q.estimate_stains(request)
    analysis=q.AnalysisRequest(doc,obj,model.transform);result=q.analyze(analysis);record=doc.keep_analysis(analysis,result)
    project=q.Project();image=project.add_document(doc);path=tmp_path/'estimated.qpxproj';project.save(str(path))
    loaded=q.Project.open(str(path)).document(image);saved=loaded.analysis_record(record)
    restored=q.StainTransform.from_normalized(saved.result.stains,saved.result.white)
    assert q.stain_model_sha256(restored)==model.model_sha256
    np.testing.assert_array_equal(saved.result.means,result.means)
