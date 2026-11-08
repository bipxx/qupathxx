"""Native QuPath TMA interchange: captured public API, ownership and atomic edits."""
import gc
import json
from pathlib import Path
import pytest
import qupathxx as q

FIXTURE=Path(__file__).resolve().parent/'fixtures/qupath-tma-0.7.0'
REFERENCE=json.loads((FIXTURE/'reference.json').read_text(encoding='utf-8'))


def source(): return (FIXTURE/'fractional-import.qptma').read_text(encoding='utf-8')
def document(): return q.Document(q.Image(256,192,[255,255,255]*(256*192)))
def core_value(core):
    b=core.bounds
    return dict(name=core.name,case_id=core.case_id,missing=core.missing,bounds=[b.x,b.y,b.width,b.height])


@pytest.mark.parametrize('filename,key',[('qupath-export.qptma','exported_then_imported'),('fractional-import.qptma','fractional_imported')])
def test_actual_qupath_wire_matches_captured_import(filename,key):
    data=q.load_qptma(str(FIXTURE/filename))
    assert (data.rows,data.columns)==(2,3)
    assert [core_value(c) for c in data.cores]==REFERENCE[key]['cores']
    assert [core_value(c) for c in q.parse_qptma(q.qptma_text(data)).cores]==REFERENCE[key]['cores']


def test_atomic_document_import_history_and_explicit_axis_labels():
    data=q.parse_qptma(source());data.source_hint='C:/absent/image-hint-must-not-be-opened.svs'
    doc=document()
    ids=q.import_qptma(doc,data,grid_name='Imported study',row_labels=['first','second'],column_labels=['left','middle','right'])
    grid=doc.tma_grid()
    assert len(ids)==6 and grid.info.name=='Imported study'
    assert grid.info.row_labels==['first','second']
    exported=q.qptma_from_grid(grid)
    assert [core_value(c) for c in exported.data.cores]==REFERENCE['created']['cores']
    assert exported.omitted_metadata==['native_object_ids','native_grid_name','axis_labels']
    revision=doc.revision
    with pytest.raises(ValueError,match='already has'): q.import_qptma(doc,data)
    assert doc.revision==revision and doc.tma_core_count==6
    assert doc.undo() and doc.tma_core_count==0
    assert doc.redo() and doc.tma_core_count==6


def test_out_of_image_last_core_leaves_document_and_history_unchanged():
    doc=document();original=doc.add_rectangle(q.Rect(1,1,2,2));revision=doc.revision
    data=q.parse_qptma(source());cores=data.cores;cores[-1].bounds=q.Rect(10000,10,20,20);data.cores=cores
    with pytest.raises(ValueError): q.import_qptma(doc,data)
    assert doc.revision==revision and doc.tma_core_count==0
    assert [a.id for a in doc.annotations]==[original]
    assert doc.undo() and not doc.annotations


def test_export_snapshot_and_core_values_have_independent_ownership():
    doc=document();q.import_qptma(doc,q.parse_qptma(source()))
    snapshot=doc.tma_grid();report=q.qptma_from_grid(snapshot);data=report.data
    changed=data.cores;changed[0].case_id='modified copy';data.cores=changed
    assert report.data.cores[0].case_id=='case-one'
    doc.clear_tma_grid(True);del doc,snapshot,report;gc.collect()
    assert data.cores[0].case_id=='modified copy'
    assert q.parse_qptma(q.qptma_text(data)).cores[0].case_id=='modified copy'


def test_utf8_and_quoted_text_survive_native_file_roundtrip_without_overwrite(tmp_path):
    data=q.parse_qptma(source());cores=data.cores
    cores[0].name='"quoted name"';cores[0].case_id=' spaced \u03b1 ';data.cores=cores
    output=tmp_path/'grid \u03b1.qptma'
    count=q.save_qptma(data,str(output));original=output.read_bytes()
    assert count==len(original)
    assert [core_value(c) for c in q.load_qptma(str(output)).cores]==[core_value(c) for c in data.cores]
    with pytest.raises(RuntimeError): q.save_qptma(data,str(output))
    assert output.read_bytes()==original
    assert list(tmp_path.iterdir())==[output]


@pytest.mark.parametrize('before,after',[
    ('TMA grid width: 3','TMA grid width: 0'),('TMA grid height: 2','TMA grid height: 513'),
    ('Core name\tX','Core name\tCore name'),('12.75','nan'),('12.75','inf'),('12.75','0x1'),
    ('12.75','1e-9999'),('17.25','-1'),('\ttrue\t','\tTRUE\t'),('A-1','A\x00-1'),
    ('A-1','A\t1'),('TMA grid height: 2','TMA grid width: 2')])
def test_malformed_wire_rejects_before_document_mutation(before,after):
    with pytest.raises(ValueError): q.parse_qptma(source().replace(before,after,1))


@pytest.mark.parametrize('suffix',['extra\n','\n'])
def test_trailing_data_rejects(suffix):
    with pytest.raises(ValueError): q.parse_qptma(source()+suffix)


def test_quotas_invalid_grid_and_output_failure_leave_no_file(tmp_path):
    data=q.parse_qptma(source())
    for call in [lambda:q.parse_qptma(source(),byte_limit=1),lambda:q.qptma_text(data,byte_limit=1),
                 lambda:q.load_qptma(str(FIXTURE/'fractional-import.qptma'),byte_limit=1)]:
        with pytest.raises(ValueError): call()
    with pytest.raises(ValueError): q.save_qptma(data,str(tmp_path/'bad.qptma'),byte_limit=1)
    data.cores=data.cores[:-1]
    with pytest.raises(ValueError): q.save_qptma(data,str(tmp_path/'bad.qptma'))
    assert not list(tmp_path.iterdir())


def test_bom_crlf_and_missing_final_newline_preserve_values():
    expected=q.parse_qptma(source())
    for text in ['\ufeff'+source().replace('\n','\r\n'),source().rstrip('\n')]:
        assert [core_value(c) for c in q.parse_qptma(text).cores]==[core_value(c) for c in expected.cores]


def test_core_list_and_input_text_admission_precede_native_copy():
    data=q.parse_qptma(source());original=[core_value(c) for c in data.cores]
    with pytest.raises(ValueError): data.cores=[q.QptmaCore()]*10001
    assert [core_value(c) for c in data.cores]==original
    invalid=q.QptmaCore();invalid.name='x'*4097
    with pytest.raises(ValueError): data.cores=[invalid]
    assert [core_value(c) for c in data.cores]==original
    with pytest.raises(ValueError): q.parse_qptma('\u03b1'*1024,byte_limit=1024)
