"""Case aggregation checked against independent CSV and rational arithmetic."""
import csv
from fractions import Fraction
import gc
import hashlib
import io
import random

import pytest
import qupathxx as q
from test_tma import scene, rows


def measurement_table(snapshot, records, *, revision=True):
    output = io.StringIO(newline='')
    writer = csv.writer(output)
    writer.writerow(['source_object_id', *(['source_object_revision'] if revision else []), 'value', 'weight'])
    cores = {core.id: core for core in snapshot.cores}
    for identity, value, weight in records:
        writer.writerow([identity, *([cores[identity].revision] if revision else []), value, weight])
    return output.getvalue()


def test_replicates_weighted_oracle_snapshot_and_unassigned_isolation():
    doc, ids = scene()
    case = 'patient,"组织"\n1'
    doc.set_tma_case_ids({ids[0]: case, ids[1]: case, ids[2]: case, ids[3]: 'unmeasured'})
    doc.set_tma_missing([ids[1]], True)
    snapshot = doc.tma_grid()
    rng = random.Random(141)
    records = [(rng.choice([ids[0], ids[2]]), rng.randint(-100, 100), rng.randint(0, 10)) for _ in range(200)]
    records += [(ids[1], 9000, 100), (ids[4], 7, 0), (ids[5], 11, 2)]
    table = measurement_table(snapshot, records)
    output = q.tma_case_summary_csv(snapshot, table, 'value', 'weight')
    result = rows(output)
    assert len(result) == 4 and all(len(row) == 24 and None not in row for row in result)
    aggregate = next(row for row in result if row['case_id'] == case)
    included = [(Fraction(value), Fraction(weight)) for identity, value, weight in records if identity in (ids[0], ids[2])]
    expected = {
        'value_sum': sum(v for v, w in included),
        'weight_sum': sum(w for v, w in included),
        'weighted_sum': sum(v*w for v, w in included),
        'mean': sum(v for v, w in included)/len(included),
        'weighted_mean': sum(v*w for v, w in included)/sum(w for v, w in included),
        'minimum': min(v for v, w in included), 'maximum': max(v for v, w in included),
    }
    for column, expected_value in expected.items():
        assert float(aggregate[column]) == pytest.approx(float(expected_value))
    assert [aggregate[k] for k in ('cores', 'present_cores', 'missing_cores', 'measured_cores', 'measurement_rows', 'ignored_missing_rows')] == ['3', '2', '1', '2', '200', '1']
    assert aggregate['input_table_sha256'] == hashlib.sha256(table.encode()).hexdigest()
    assert aggregate['measurement_revision_status'] == 'verified'
    unassigned = {r['unassigned_core_id']: r for r in result if not r['case_id']}
    assert set(unassigned) == {str(ids[4]), str(ids[5])}
    assert unassigned[str(ids[4])]['weighted_mean'] == ''
    assert unassigned[str(ids[4])]['mean'] == '7'
    unmeasured = next(r for r in result if r['case_id'] == 'unmeasured')
    assert unmeasured['mean'] == unmeasured['value_sum'] == ''
    doc.set_tma_case_ids({ids[0]: 'edited'})
    del doc
    gc.collect()
    assert q.tma_case_summary_csv(snapshot, table, 'value', 'weight') == output


def test_inventory_counts_vacancy_without_inventing_measurements():
    doc, ids = scene()
    doc.set_tma_case_ids({ids[0]: 'replicate', ids[1]: 'replicate'})
    doc.set_tma_missing([ids[1]], True)
    doc.remove_annotation(ids[5])
    result = rows(q.tma_case_summary_csv(doc.tma_grid()))
    assert len(result) == 4
    assert all(r['grid_slots'] == '6' and r['grid_vacant_slots'] == '1' for r in result)
    assert sum(int(r['cores']) for r in result) == 5
    assert all(r['mean'] == r['input_table_sha256'] == '' for r in result)
    assert all(r['measurement_revision_status'] == 'no_measurements' for r in result)


def test_revision_opt_in_never_bypasses_a_supplied_stale_revision():
    doc, ids = scene()
    snapshot = doc.tma_grid()
    table = measurement_table(snapshot, [(ids[0], 2, 1)], revision=False)
    with pytest.raises(ValueError):
        q.tma_case_summary_csv(snapshot, table, 'value')
    result = rows(q.tma_case_summary_csv(snapshot, table, 'value', allow_unversioned=True))
    assert next(r for r in result if r['measurement_rows'] == '1')['measurement_revision_status'] == 'unversioned'
    versioned = measurement_table(snapshot, [(ids[0], 2, 1)])
    doc.set_tma_missing([ids[0]], True)
    for allow in (False, True):
        with pytest.raises(ValueError, match='revision'):
            q.tma_case_summary_csv(doc.tma_grid(), versioned, 'value', allow_unversioned=allow)


@pytest.mark.parametrize('value,weight', [('nan', '1'), ('inf', '1'), ('2', '-1'), ('1e308', '1e308'), ('1e-300', '1e-300'), (' 1', '1'), ('1', 'NaN')])
def test_rejects_invalid_or_unrepresentable_measurements(value, weight):
    doc, ids = scene()
    snapshot = doc.tma_grid()
    table = measurement_table(snapshot, [(ids[0], value, weight)])
    with pytest.raises((ValueError, RuntimeError, OverflowError)):
        q.tma_case_summary_csv(snapshot, table, 'value', 'weight')


def test_selectors_malformed_csv_and_byte_limits():
    doc, ids = scene()
    snapshot = doc.tma_grid()
    table = measurement_table(snapshot, [(ids[0], 3, 2)])
    for invalid in [table.rstrip(), table.replace('value,weight', 'value,value'), table.replace(f'{ids[0]},', '999999,'), table + '"unterminated']:
        with pytest.raises(ValueError):
            q.tma_case_summary_csv(snapshot, invalid, 'value')
    with pytest.raises((ValueError, RuntimeError)):
        q.tma_case_summary_csv(snapshot, table, 'value', byte_limit=len(table.encode())-1)
    with pytest.raises((ValueError, RuntimeError)):
        q.tma_case_summary_csv(snapshot, byte_limit=1)
    for kwargs in [dict(value_column='value'), dict(weight_column='weight'), dict(table=table)]:
        with pytest.raises(ValueError):
            q.tma_case_summary_csv(snapshot, **kwargs)


def test_signed_cancellation_keeps_small_replicate_contribution():
    doc, ids = scene()
    snapshot = doc.tma_grid()
    table = measurement_table(snapshot, [(ids[0], '1e16', 1), (ids[0], 1, 1), (ids[0], '-1e16', 1)])
    result = next(r for r in rows(q.tma_case_summary_csv(snapshot, table, 'value', 'weight')) if r['measurement_rows'] == '3')
    assert float(result['value_sum']) == float(result['weighted_sum']) == 1
    assert float(result['mean']) == pytest.approx(1/3)


def test_mean_underflow_cannot_silently_become_zero():
    doc, ids = scene()
    snapshot = doc.tma_grid()
    table = measurement_table(snapshot, [(ids[0], '1e-323', 1), *[(ids[0], 0, 1)]*4])
    with pytest.raises((RuntimeError, OverflowError)):
        q.tma_case_summary_csv(snapshot, table, 'value', 'weight')
