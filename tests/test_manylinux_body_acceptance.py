"""Portable body acceptance must reject missing, failed or skipped evidence."""
import importlib.util
from pathlib import Path
import xml.etree.ElementTree as ET

import pytest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('qpx_portable_body_audit',ROOT/'tools/ci/accept_manylinux.py')
audit=importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def evidence(folder):
    contract=audit.read(ROOT/'tests/fixtures/qupath-cells-0.7.0/contract.json')
    names=[f"test_production_body_mode_pinned_labels_and_measurements[{tile}-{case['name']}]"
           for tile in contract['tile_sizes'] for case in contract['cases']]
    names += [f'test_body_worker_rejects_stale_authority_and_cancellation[{change}]'
              for change in ['selection','rename','close','authority','cancel']]
    names += [f'test_body_mode_natural_level_identity_and_work_admission[{level}]' for level in range(3)]
    desktop=['test_body_gui_captured_recipe_and_published_pixels',
             'test_body_managed_viewer_copies_recipe_and_reopens_retained_result',
             'test_body_gui_batch_keeps_distinct_recipe']
    names += ['test_body_archive_replay_classifier_and_legacy_records',
              'test_body_batch_mode_has_distinct_resume_and_cli_recipe',
              'test_body_candidate_limit_failure_keeps_document_and_worker_reusable',
              'test_body_expansion_respects_transparent_source_domain',*desktop]
    paths=[]
    for version in ['3.11','3.12']:
        build=folder/('manylinux-'+version)
        consumer=folder/('manylinux-consumer-'+version)
        build.mkdir();consumer.mkdir()
        native=ET.Element('testsuite')
        ET.SubElement(native,'testcase',name='cell_expansion')
        ET.ElementTree(native).write(build/'native-tests.xml')
        for path in [build/'python-tests.xml',consumer/'consumer-tests.xml']:
            suite=ET.Element('testsuite')
            for name in names:
                case=ET.SubElement(suite,'testcase',classname='tests.test_log_body_cells',name=name)
                if name in desktop:
                    ET.SubElement(case,'skipped',type='pytest.skip',message='requires desktop')
            ET.ElementTree(suite).write(path)
            paths.append(path)
    return paths


def test_all_four_portable_body_cohorts_are_required(tmp_path):
    evidence(tmp_path)
    result=audit.watershed_body_cohorts(tmp_path)
    assert len(result)==4
    assert all(r['passed']==39 and r['desktop_skipped']==3 for r in result)


@pytest.mark.parametrize('change',['missing_case','renamed_case','failed_consumer',
                                   'skipped_archive','desktop_xfail','missing_native','native_skip'])
def test_incomplete_portable_body_evidence_rejects(tmp_path,change):
    paths=evidence(tmp_path)
    path=paths[-1]
    tree=ET.parse(path);root=tree.getroot()
    if change=='missing_case':
        root.remove(root[0])
    elif change=='renamed_case':
        root[0].set('name','wrong_case')
    elif change=='failed_consumer':
        ET.SubElement(root[0],'failure')
    elif change=='skipped_archive':
        case=next(t for t in root if t.get('name')=='test_body_archive_replay_classifier_and_legacy_records')
        ET.SubElement(case,'skipped',type='pytest.skip')
    elif change=='desktop_xfail':
        next(root.iter('skipped')).set('type','pytest.xfail')
    else:
        path=tmp_path/'manylinux-3.12/native-tests.xml'
        tree=ET.parse(path);root=tree.getroot()
        if change=='missing_native':
            root.remove(root[0])
        else:
            ET.SubElement(root[0],'skipped')
    tree.write(path)
    with pytest.raises(ValueError):
        audit.watershed_body_cohorts(tmp_path)
