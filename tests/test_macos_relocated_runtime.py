"""Independent scalar/format/mapping contracts; never run native Mac fixtures."""
import csv
import importlib.util
import io
import json
import math
from pathlib import Path
import shutil
import sqlite3
import subprocess

import pytest

spec=importlib.util.spec_from_file_location('qpx_mac_runtime_tests',Path(__file__).resolve().parents[1]/'tools/ci/check_macos_relocated_runtime.py')
runtime=importlib.util.module_from_spec(spec);spec.loader.exec_module(runtime)


def csv_text(**changes):
    row=dict(table_schema='qpx.roi-table/1',algorithm='qpx.roi-channel-means/1',status_at_capture='current',
             channel_space='optical_density_rgb',batch_image_id='1',source_object_id='1',samples='4',roi_area_px2='4',
             roi_area_um2='0.5',roi_length_um='3',captured_pixel_width_um='0.5',captured_pixel_height_um='0.25',
             roi_min_x_px='0',roi_min_y_px='0',roi_width_px='2',roi_height_px='2',nucleus='0',roi_kind='0',
             white_r='255',white_g='255',white_b='255',
             source_tree_sha256='a'*64)
    row.update({'mean_channel_'+str(i):str(math.log10(255/value)) for i,value in enumerate([30,60,90],1)})
    row.update(changes)
    output=io.StringIO();writer=csv.DictWriter(output,fieldnames=list(row));writer.writeheader();writer.writerow(row)
    return output.getvalue()


def test_independent_log10_roi_calibration_and_finite_tolerance():
    summary=runtime.csv_contract(csv_text())
    assert summary['samples']==4 and summary['roi_area_um2']==0.5 and summary['roi_length_um']==3
    assert summary['means']==[math.log10(255/value) for value in [30,60,90]]
    assert summary['absolute_tolerance']==2e-7
    assert runtime.csv_contract(csv_text(mean_channel_1=str(math.log10(8.5)+1e-7)))['rows']==1


@pytest.mark.parametrize('changes',[dict(samples='5'),dict(roi_area_um2='4'),dict(roi_length_um='8'),
    dict(captured_pixel_height_um='0.5'),dict(status_at_capture='stale'),dict(batch_image_id='2'),
    dict(source_object_id='2'),dict(channel_space='stains'),dict(table_schema='other'),
    dict(mean_channel_1='NaN'),dict(mean_channel_2='inf'),dict(mean_channel_3='-inf'),
    dict(mean_channel_1=str(math.log(8.5))),dict(mean_channel_1=str(math.log10(8.5)+1e-5)),dict(source_tree_sha256='unknown'),
    dict(white_r='256')])
def test_changed_numerics_ids_algorithms_and_nonfinite_values_reject(changes):
    with pytest.raises(ValueError):runtime.csv_contract(csv_text(**changes))


def test_csv_duplicate_columns_and_additional_rows_reject():
    data=csv_text()
    with pytest.raises(ValueError):runtime.csv_contract(data+data.splitlines()[1]+'\n')
    lines=data.splitlines();lines[0]+=',samples';lines[1]+=',4'
    with pytest.raises(ValueError):runtime.csv_contract('\n'.join(lines))


def batch_text(**changes):
    image=dict(schema='qpx.batch-image/1',image_id=1,state='saved',objects=1,records=1,error='')
    report=dict(schema='qpx.project-batch/1',state='succeeded',saved=1,failed=0,reused=0,source_identity='fixture',images=[image])
    report.update(changes)
    return json.dumps(image)+'\n'+json.dumps(report)+'\n'


def test_batch_checkpoint_selection_and_matching_jsonl():
    assert runtime.batch_contract(batch_text())==dict(saved=1,failed=0,records=1,source_identity='fixture')


@pytest.mark.parametrize('changes',[dict(state='failed'),dict(saved=2),dict(failed=1),dict(reused=1),dict(images=[])])
def test_batch_failure_or_partial_cohort_cannot_pass(changes):
    with pytest.raises(ValueError):runtime.batch_contract(batch_text(**changes))


def test_boolean_counts_cannot_impersonate_saved_integer_records():
    with pytest.raises(ValueError):runtime.batch_contract(batch_text(saved=True))
    image,summary=batch_text().splitlines();summary=json.loads(summary);image=json.loads(image)
    image['records']=True;summary['images'][0]['records']=True
    with pytest.raises(ValueError):runtime.batch_contract(json.dumps(image)+'\n'+json.dumps(summary))


def test_batch_disagreement_duplicate_keys_and_truncated_log_reject():
    text=batch_text();image,report=text.splitlines()
    with pytest.raises(ValueError):runtime.batch_contract(report)
    with pytest.raises(ValueError):runtime.batch_contract(text.replace('"saved": 1','"saved": 1, "saved": 1'))
    altered=json.loads(image);altered['records']=2
    with pytest.raises(ValueError):runtime.batch_contract(json.dumps(altered)+'\n'+report)


def test_dyld_mapping_parser_preserves_spaces_and_works_off_host():
    text='dyld[123]: <UUID> /Users/runner/fresh package with spaces/lib/libgeos_c.1.dylib\n'
    text+='dyld: loaded: /Users/runner/fresh package with spaces/lib/libgeos.3.15.0.dylib\n'
    text+='non-dyld diagnostic /wrong/libgeos_c.1.dylib\n'
    assert runtime.loaded_from_diagnostics(text,{'libgeos_c.1.dylib','libgeos.3.15.0.dylib'})==[
        '/Users/runner/fresh package with spaces/lib/libgeos.3.15.0.dylib',
        '/Users/runner/fresh package with spaces/lib/libgeos_c.1.dylib']
    assert runtime.loaded_from_diagnostics('',{'libgeos_c.1.dylib'})==[]


def test_graph_closure_is_cycle_safe_and_keeps_only_libraries():
    plan=dict(dependency_edges=[dict(source='bin/a',target='lib/a'),dict(source='lib/a',target='lib/b'),
                                dict(source='lib/b',target='lib/a'),dict(source='lib/b',target=None)])
    assert runtime.closure(plan,'bin/a')==['lib/a','lib/b']


def test_non_mac_guards_before_loading_or_extraction(tmp_path,monkeypatch):
    monkeypatch.setattr(runtime.platform,'system',lambda:'Windows')
    with pytest.raises(ValueError):runtime.geometry_child(tmp_path/'not-a-library',tmp_path/'geometry.json')
    with pytest.raises(ValueError):runtime.probe(tmp_path/'archive',dict(architecture='arm64'),tmp_path/'fixture',tmp_path/'work',tmp_path/'out.json')
    assert not (tmp_path/'work').exists() and not (tmp_path/'geometry.json').exists()


def test_checked_in_legacy_project_fixture_and_changed_pixel_control(tmp_path):
    fixture=Path(__file__).resolve().parent/'fixtures/project-v1.qpxproj'
    observed=runtime.fixture_contract(fixture)
    assert observed['identity']=='c75c6b9f10de06bcb03787f54a3f2ea9'
    target=tmp_path/'changed.qpxproj';shutil.copy2(fixture,target)
    with sqlite3.connect(target) as connection:connection.execute("UPDATE images SET rgb=? WHERE id='1'",(bytes([31,60,90])*4,))
    with pytest.raises(ValueError):runtime.fixture_contract(target)


def test_full_runtime_orchestration_with_labelled_simulated_commands(tmp_path,monkeypatch):
    from test_macos_relocation_plan import snapshot,ROOTS
    from test_macos_relocate import stage_fixture
    original=snapshot(tmp_path,native_dependencies=((0xc,'@rpath/libgeos_c.1.dylib'),),
        capi_id='@rpath/libgeos_c.1.dylib',cpp_id='@rpath/libgeos.3.15.0.dylib',
        capi_dependencies=((0xc,'@rpath/libgeos.3.15.0.dylib'),))
    plan=runtime.relocation.planner.create_plan(original,'arm64',ROOTS)
    stage=stage_fixture(tmp_path,original,plan)
    archive=tmp_path/'relocated.zip';runtime.relocation.pack(stage,plan,archive)
    fixture=Path(__file__).resolve().parent/'fixtures/project-v1.qpxproj'
    identity=runtime.fixture_contract(fixture)['identity'];work=tmp_path/'runtime';output=tmp_path/'runtime.json'
    monkeypatch.setattr(runtime.platform,'system',lambda:'Darwin')
    monkeypatch.setattr(runtime.platform,'machine',lambda:'arm64')
    # This adapter only represents fake process diagnostics on Windows. The
    # actual POSIX dyld format/parser has independent tests above.
    monkeypatch.setattr(runtime,'loaded_from_diagnostics',lambda text,names:sorted(json.loads(text)))
    calls=[]
    def run(arguments,**options):
        arguments=list(map(str,arguments));calls.append(arguments)
        assert 'LD_LIBRARY_PATH' not in options['env'] and 'DYLD_LIBRARY_PATH' not in options['env']
        package=work/'fresh relocated package with spaces'
        if '--geometry-child' in arguments:
            library=Path(arguments[arguments.index('--geometry-child')+1])
            paths=[package/key for key in runtime.closure(plan,'lib/libgeos_c.1.dylib')]
            geometry=dict(schema='qpx.macos-geos-relocated-child/1',library=str(library.resolve()),
                geos_version='simulated-only',union_area=150,mapped=[dict(path=str(p.resolve()),sha256=runtime.sha(p)) for p in paths],release_ready=False)
            target=Path(arguments[-1]);target.write_bytes(runtime.common.canonical_json(geometry))
            return subprocess.CompletedProcess(arguments,0,json.dumps(geometry).encode('utf-8'),b'')
        mappings=[str((package/key).resolve()) for key in runtime.closure(plan,'bin/'+Path(arguments[0]).name)]
        stderr=json.dumps(mappings).encode('utf-8')
        assert options['env']['DYLD_PRINT_LIBRARIES']=='1'
        if '--thumbnail' in arguments:
            Path(arguments[5]).write_bytes(b'P7\nENDHDR\n'+bytes([60,100,180,255]))
            return subprocess.CompletedProcess(arguments,0,b'',stderr)
        if '--export-csv' in arguments:
            Path(arguments[-1]).write_text(csv_text(),encoding='utf-8')
            return subprocess.CompletedProcess(arguments,0,b'',stderr)
        directory=work/'batch/selected';directory.mkdir(parents=True)
        with sqlite3.connect(directory/'checkpoint.qpxproj') as db:
            db.execute('CREATE TABLE analysis_records(id INTEGER)');db.execute('INSERT INTO analysis_records VALUES(1)')
        text=batch_text(source_identity=identity)
        return subprocess.CompletedProcess(arguments,0,text.encode('utf-8'),stderr)
    monkeypatch.setattr(runtime.subprocess,'run',run)
    before=fixture.read_bytes();result=runtime.probe(archive,plan,fixture,work,output)
    assert len(calls)==4 and result['batch']['records']==1 and result['geometry']['union_area']==150
    assert result['all_package_bytes_preserved'] and result['fixture_preserved'] and fixture.read_bytes()==before
    assert not result['loader_search_overrides_used'] and not result['release_ready']
