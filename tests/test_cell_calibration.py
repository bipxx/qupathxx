"""Physical sampled-cell values use immutable capture-time calibration."""
import csv
import io
from pathlib import Path
import sqlite3
import subprocess

import numpy as np
import pytest
import qupathxx as q
from test_tissue_levels import pyramid_levels


def scene(calibration=(.5,.25),level=0,server=None):
    if server is None:
        rgb=np.full((40,48,3),245,dtype=np.uint8)
        rgb[8:18,7:15]=[35,45,75];rgb[22:31,29:39]=[45,25,90]
        doc=q.Document(q.Image.from_numpy(rgb))
    else:
        doc=q.Document(server)
    if calibration is not None:doc.set_calibration(*calibration)
    obj=doc.add_rectangle(q.Rect(0,0,doc.metadata.width,doc.metadata.height))
    parameters=q.CellParameters();parameters.sigma=0;parameters.threshold=.5
    parameters.min_nucleus_pixels=1;parameters.cell_expansion=3;parameters.level=level
    request=q.CellRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]),parameters)
    result=q.detect_cells(request);record=doc.keep_cells(request,result)
    return doc,obj,result,record


def options(published=False):
    value=q.CellTableOptions();value.include_capture_calibration=True;value.published_objects=published
    return value


def rows(text):
    reader=csv.DictReader(io.StringIO(text,newline=''));values=list(reader)
    assert len(reader.fieldnames)==len(set(reader.fieldnames))
    assert all(None not in row for row in values)
    return values


def check_samples(result,calibration):
    assert result.cell_count>0
    scale=result.nuclei.downsample
    for label in range(1,result.cell_count+1):
        value=result.calibrated_feature(label)
        assert (value.pixel_width_um,value.pixel_height_um)==calibration
        for raster,prefix in [(result.nuclei,'nucleus'),(result.cells,'cell')]:
            yy,xx=np.where(raster.labels_numpy()==label)
            x=(xx.mean()+raster.level_bounds.x+.5)*scale*calibration[0]
            y=(yy.mean()+raster.level_bounds.y+.5)*scale*calibration[1]
            point=getattr(value,prefix+'_centroid_um')
            assert (point.x,point.y)==pytest.approx((x,y),rel=1e-12,abs=1e-12)
            assert getattr(value,prefix+'_sample_area_um2')==pytest.approx(len(xx)*scale**2*calibration[0]*calibration[1],rel=1e-12)
        assert value.cytoplasm_sample_area_um2==pytest.approx(value.cell_sample_area_um2-value.nucleus_sample_area_um2,abs=1e-12)


def test_anisotropic_snapshot_stays_fixed_after_calibration_changes_and_source_close():
    doc,obj,result,record=scene();check_samples(result,(.5,.25))
    first=result.calibrated_feature(1)
    doc.set_calibration(2,3);doc.rename_annotation(obj,'Changed');doc.server.close()
    assert result.calibrated_feature(1).nucleus_sample_area_um2==first.nucleus_sample_area_um2
    assert result.calibrated_feature(1).pixel_width_um==.5
    assert doc.cell_record(record).result.calibrated_feature(1).pixel_height_um==.25
    point=first.nucleus_centroid_um
    point.x=12345
    assert result.calibrated_feature(1).nucleus_centroid_um.x!=12345
    with pytest.raises(AttributeError):first.pixel_width_um=42
    for label in [0,result.cell_count+1]:
        with pytest.raises(IndexError):result.calibrated_feature(label)


def test_unknown_capture_has_no_invented_units():
    doc,obj,result,record=scene(None)
    doc.set_calibration(.5,.25)
    value=result.calibrated_feature(1)
    assert value.pixel_width_um is value.pixel_height_um is None
    assert value.nucleus_sample_area_um2 is value.cell_sample_area_um2 is value.cytoplasm_sample_area_um2 is None
    assert value.nucleus_centroid_um is value.cell_centroid_um is None
    assert all(row['nucleus_sample_area_um2']==row['capture_pixel_width_um']=='' for row in rows(q.cell_measurements_csv(doc,options=options())))


def test_opt_in_csv_version_and_snapshot_are_explicit():
    doc,obj,result,record=scene()
    old=rows(q.cell_measurements_csv(doc));assert old[0]['table_schema']=='qpx.cell-table/1'
    assert 'capture_pixel_width_um' not in old[0]
    capture=q.CellTableRequest(doc,options=options());csv_text=q.cell_measurements_csv(capture)
    new=rows(csv_text);assert new[0]['table_schema']=='qpx.cell-table/2'
    assert len(new[0])==len(old[0])+9
    for a,b in zip(old,new,strict=True):
        assert all(a[key]==b[key] for key in a if key!='table_schema')
        feature=result.calibrated_feature(int(b['cell_label']))
        assert float(b['nucleus_sample_area_um2'])==feature.nucleus_sample_area_um2
    doc.set_calibration(2,3);doc.remove_cells(record)
    assert q.cell_measurements_csv(capture)==csv_text


@pytest.mark.skipif(not q.has_project,reason='project persistence')
def test_project_reopen_and_legacy_unknown_do_not_use_live_calibration(tmp_path):
    doc,obj,result,record=scene();doc.set_calibration(2,3)
    project=q.Project();image=project.add_document(doc);path=tmp_path/'physical.qpxproj';project.save(str(path))
    reopened=q.Project.open(str(path));restored=reopened.document(image).cell_record(record).result
    check_samples(restored,(.5,.25))
    table=rows(q.cell_measurements_csv(reopened,options=options()))
    assert table[0]['table_schema']=='qpx.project-cell-table/2' and float(table[0]['capture_pixel_width_um'])==.5
    with sqlite3.connect(path) as db:db.execute('DELETE FROM analysis_provenance')
    legacy=q.Project.open(str(path)).document(image).cell_record(record).result
    assert legacy.calibrated_feature(1).nucleus_sample_area_um2 is None


@pytest.mark.skipif(not q.has_geos,reason='cell publication')
def test_joined_csv_distinguishes_capture_and_live_geometry_units():
    doc,obj,result,record=scene();doc.publish_cell_record(record)
    manual=doc.add_cell(q.Roi.rectangle(q.Rect(1,1,2,2)))
    doc.set_calibration(2,3)
    table=rows(q.cell_measurements_csv(doc,options=options(True)))
    assert all(row['table_schema']=='qpx.cell-object-table/2' for row in table)
    archived=[row for row in table if int(row['object_id'])!=manual]
    assert all(float(row['capture_pixel_width_um'])==.5 and float(row['export_pixel_width_um'])==2 for row in archived)
    missing=next(row for row in table if int(row['object_id'])==manual)
    assert missing['capture_pixel_width_um']==missing['cell_sample_area_um2']==''


@pytest.mark.skipif(not q.has_geos or not q.has_project,reason='published project tables')
def test_project_joined_capture_schema_survives_reopen(tmp_path):
    doc,_,result,record=scene();doc.publish_cell_record(record);doc.set_calibration(2,3)
    project=q.Project();image=project.add_document(doc)
    path=tmp_path/'published.qpxproj';project.save(str(path))
    table=rows(q.cell_measurements_csv(q.Project.open(str(path)),options=options(True)))
    assert len(table)==result.cell_count
    assert all(row['table_schema']=='qpx.project-cell-object-table/2' and int(row['image_id'])==image for row in table)
    assert all(float(row['capture_pixel_width_um'])==.5 and float(row['export_pixel_width_um'])==2 for row in table)


@pytest.mark.parametrize('level',[1,2])
def test_native_pyramid_grid_uses_sample_area_and_level_zero_centroids(pyramid_levels,level):
    server,_=pyramid_levels
    _,_,result,_=scene(level=level,server=server)
    check_samples(result,(.5,.25))


def test_unrepresentable_calibration_is_atomic_for_csv(tmp_path):
    doc,obj,result,record=scene((1e300,1e300))
    destination=tmp_path/'output.csv';destination.write_bytes(b'existing')
    with pytest.raises(OverflowError):q.save_cell_measurements_csv(doc,str(destination),options=options())
    assert destination.read_bytes()==b'existing' and not list(tmp_path.glob('*.qpx-tmp-*'))


def test_fractional_pyramid_area_preserves_subnormal_intermediates(pyramid_levels):
    from decimal import Decimal, localcontext
    import math
    server,_=pyramid_levels
    sx=math.nextafter(1e-320,math.inf)
    _,_,result,_=scene((sx,1e300),level=1,server=server)
    assert any(f.nucleus_pixels%4 for f in result.features)
    with localcontext() as context:
        context.prec=80
        unit=Decimal.from_float(1.5)**2*Decimal.from_float(sx)*Decimal.from_float(1e300)
        for f in result.features:
            actual=result.calibrated_feature(f.label)
            assert actual.nucleus_sample_area_um2==pytest.approx(float(f.nucleus_pixels*unit),rel=1e-14,abs=0)
            assert actual.cell_sample_area_um2==pytest.approx(float(f.cell_pixels*unit),rel=1e-14,abs=0)


@pytest.mark.skipif(not q.has_project,reason='project CLI export')
def test_native_cell_table_cli_reproduces_api_and_protects_input(tmp_path):
    from qupathxx import _native
    doc,_,_,_=scene();project=q.Project();project.add_document(doc)
    source=tmp_path/'input.qpxproj';project.save(str(source));destination=tmp_path/'output.csv'
    executable=Path(_native.__file__).parent/('qupathxx_batch.exe' if __import__('os').name=='nt' else 'qupathxx_batch')
    command=[str(executable),'--export-cell-table',str(source),str(destination),'--capture-calibration']
    done=subprocess.run(command,capture_output=True,timeout=60)
    assert done.returncode==0,done.stderr
    expected=q.cell_measurements_csv(q.Project.open(str(source)),options=options())
    assert destination.read_bytes()==expected.encode()
    before=source.read_bytes();command[3]=str(source)
    assert subprocess.run(command,capture_output=True,timeout=60).returncode!=0
    assert source.read_bytes()==before


@pytest.mark.skipif(not q.has_project,reason='project CLI export')
def test_native_cell_table_cli_rejects_aliases_and_bad_flags(tmp_path):
    from qupathxx import _native
    doc,_,_,_=scene();project=q.Project();project.add_document(doc)
    source=tmp_path/'input.qpxproj';project.save(str(source))
    executable=Path(_native.__file__).parent/('qupathxx_batch.exe' if __import__('os').name=='nt' else 'qupathxx_batch')
    alias=tmp_path/'alias.csv'
    try:alias.hardlink_to(source)
    except OSError as error:pytest.skip(f'hard links unavailable: {error}')
    before=source.read_bytes()
    base=[str(executable),'--export-cell-table',str(source)]
    assert subprocess.run([*base,str(alias)],capture_output=True,timeout=60).returncode!=0
    assert source.read_bytes()==alias.read_bytes()==before
    output=tmp_path/'sentinel.csv';output.write_bytes(b'previous')
    for flags in [['--unknown'],['--capture-calibration','--capture-calibration'],
                  ['--published','--published'],['--require-current','--require-current']]:
        assert subprocess.run([*base,str(output),*flags],capture_output=True,timeout=60).returncode!=0
        assert output.read_bytes()==b'previous'


@pytest.mark.skipif(not q.has_project,reason='project CLI export')
def test_native_cell_table_cli_protects_linked_image_and_reads_missing_history(tmp_path):
    from qupathxx import _native
    source=tmp_path/'pixels.ppm'
    rgb=np.full((24,32,3),245,dtype=np.uint8);rgb[6:16,8:16]=16
    source.write_bytes(b'P6\n32 24\n255\n'+rgb.tobytes())
    project=q.Project();image=project.add_image(str(source),reader='ppm')
    doc=project.document(image);doc.set_calibration(.5,.25)
    obj=doc.add_rectangle(q.Rect(0,0,32,24))
    p=q.CellParameters();p.sigma=0;p.min_nucleus_pixels=1
    request=q.CellRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]),p)
    doc.keep_cells(request,q.detect_cells(request))
    storage=tmp_path/'linked.qpxproj';project.save(str(storage));before=source.read_bytes()
    executable=Path(_native.__file__).parent/('qupathxx_batch.exe' if __import__('os').name=='nt' else 'qupathxx_batch')
    command=[str(executable),'--export-cell-table',str(storage),str(source),'--capture-calibration']
    assert subprocess.run(command,capture_output=True,timeout=60).returncode!=0
    assert source.read_bytes()==before
    doc.server.close();source.unlink()
    output=tmp_path/'history.csv';command[3]=str(output)
    done=subprocess.run(command,capture_output=True,timeout=60)
    assert done.returncode==0,done.stderr
    assert output.read_bytes()==q.cell_measurements_csv(q.Project.open(str(storage)),options=options()).encode()
