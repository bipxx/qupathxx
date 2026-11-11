"""Development-only independent oracle/data transcode; never a product runtime.

Run with NumPy/SciPy installed separately. The native MTX importer, A02 and A04
remain the authorities for product interpretation, reduction and statistics.
"""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import warnings
import zipfile


def statistics(counts, groups, target=10000):
    import numpy as np
    from scipy import stats
    raw = np.asarray(counts, dtype=np.uint64)
    values = np.log1p(raw.astype(float) / raw.sum(axis=0).astype(float) * target)
    a, b = values[:, np.asarray(groups) == "A"], values[:, np.asarray(groups) == "B"]
    means = np.column_stack((a.mean(axis=1), b.mean(axis=1)))
    variances = np.column_stack((a.var(axis=1, ddof=1), b.var(axis=1, ddof=1)))
    testable = (np.ptp(a, axis=1) != 0) | (np.ptp(b, axis=1) != 0)
    rows = []
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", RuntimeWarning)
        # B first to obtain the documented B-minus-A signed t/effect.
        tested = stats.ttest_ind(b[testable], a[testable], axis=1, equal_var=False)
    adjusted = stats.false_discovery_control(tested.pvalue, method="bh")
    slot = 0
    for f in range(len(raw)):
        row = dict(mean_a=float(means[f, 0]), mean_b=float(means[f, 1]),
                   variance_a=float(variances[f, 0]), variance_b=float(variances[f, 1]),
                   effect=float(means[f, 1] - means[f, 0]))
        if testable[f]:
            row.update(t=float(tested.statistic[slot]), df=float(tested.df[slot]),
                       p=float(tested.pvalue[slot]), q=float(adjusted[slot]), status="tested")
            slot += 1
        else:
            row.update(t=None, df=None, p=None, q=None,
                       status="all-zero" if not raw[f].any() else "zero-variance")
        rows.append(row)
    return rows


def frozen(path):
    import numpy as np
    import scipy
    raw = [[1, 3, 2, 9, 11, 12], [20, 10, 15, 5, 7, 6], [0]*6,
           [5]*6, [2, 2, 2, 4, 5, 6], [1, 4, 2, 1, 4, 2]]
    raw.append([100-sum(row[r] for row in raw) for r in range(6)])
    # Split replicate0 and3 into independent disjoint cohort columns. Native
    # A04 must sum these first; normalizing cohorts separately changes the answer.
    observed = []
    for row in raw:
        observed.append([row[0]//3, row[0]-row[0]//3, row[1], row[2],
                         row[3]//2, row[3]-row[3]//2, row[4], row[5]])
    result = dict(schema="qpx.expression-differential-oracle/1", numpy=np.__version__,
                  scipy=scipy.__version__, target=10000, counts=observed,
                  replicate_counts=raw, groups=["A"]*3+["B"]*3,
                  observation_replicates=[0, 0, 1, 2, 3, 3, 4, 5],
                  expected=statistics(raw, ["A"]*3+["B"]*3))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(result, indent=2, allow_nan=False)+"\n", encoding="utf8")


def prepare(zip_path, metadata, output):
    """Lossless original-author Kidney CSV -> existing open MTX/TSV fixture route.

    No QC, filtering, normalization, aggregation or inference of biological units.
    Every source cell/gene/count is retained; mouse and sex come from author table.
    """
    output.mkdir(exist_ok=False)
    plates = {r['plate.barcode']: r for r in csv.DictReader(metadata.open(encoding="utf8"))
              if r['tissue'] == 'Kidney'}
    with zipfile.ZipFile(zip_path) as z:
        data = z.read('FACS/Kidney-counts.csv')
    (output/'Kidney-counts.csv').write_bytes(data)
    reader = csv.reader(io.StringIO(data.decode('utf8')))
    header = next(reader)
    observations = header[1:]
    assert len(set(observations)) == len(observations) and not header[0]
    cohorts = {p: dict(id=p, sample=r['mouse.id'], replicate=r['mouse.id'],
                      group=r['mouse.sex'], observations=[]) for p, r in plates.items()}
    for cell in observations:
        tokens = cell.split('.')
        assert len(tokens) == 5 and tokens[1] in plates
        assert tokens[2] == plates[tokens[1]]['mouse.id']
        cohorts[tokens[1]]['observations'].append(cell)
    features, nnz, totals = [], 0, [0]*len(observations)
    with (output/'entries.tmp').open('w', encoding='utf8', newline='\n') as entries:
        for f, row in enumerate(reader):
            assert len(row) == len(header) and row[0] and row[0] not in features
            features.append(row[0])
            for c, token in enumerate(row[1:]):
                assert token.isascii() and token.isdecimal()
                count = int(token)
                assert 0 <= count <= 2**64-1
                totals[c] += count
                if count:
                    entries.write(f'{f+1} {c+1} {count}\n')
                    nnz += 1
    with (output/'matrix.mtx').open('w', encoding='utf8', newline='\n') as matrix:
        matrix.write(f'%%MatrixMarket matrix coordinate integer general\n{len(features)} {len(observations)} {nnz}\n')
        with (output/'entries.tmp').open(encoding='utf8') as entries:
            for line in entries:
                matrix.write(line)
    (output/'features.tsv').write_text('feature_id\n'+'\n'.join(features)+'\n', encoding='utf8')
    (output/'observations.tsv').write_text('observation_id\n'+'\n'.join(observations)+'\n', encoding='utf8')
    spec = dict(schema='qpx.expression-aggregation-spec/1', kind='pseudobulk', overlap='reject',
                cohorts=list(cohorts.values()))
    (output/'cohorts.json').write_text(json.dumps(spec, indent=2)+'\n', encoding='utf8')
    receipt = dict(source='figshare5829687v8/FACS/Kidney-counts.csv', license='CC-BY-4.0',
                   source_csv_sha256=hashlib.sha256(data).hexdigest(), features=len(features),
                   observations=len(observations), nnz=nnz, total_counts=sum(totals),
                   per_cell_raw_totals=totals, cohorts=list(cohorts.values()),
                   scope='all source kidney genes/cells; 4 male and 2 female independent mice; engineering method check')
    (output/'transcode.json').write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf8')


def verify(raw_csv, metadata, native_csv, replicates_csv, output):
    import numpy as np
    import scipy
    plates = {r['plate.barcode']: r for r in csv.DictReader(metadata.open(encoding='utf8'))}
    native_replicates = list(csv.DictReader(replicates_csv.open(encoding='utf8')))
    ids = list(dict.fromkeys(r['replicate_id'] for r in native_replicates))
    # Association is independently joined from source plate metadata/column IDs.
    with raw_csv.open(encoding='utf8') as f:
        reader = csv.reader(f)
        cells = next(reader)[1:]
        slots = [ids.index(plates[c.split('.')[1]]['mouse.id']) for c in cells]
        matrix, features = [], []
        for row in reader:
            features.append(row[0])
            sums = [0]*len(ids)
            for c, token in enumerate(row[1:]):
                sums[slots[c]] += int(token)
            matrix.append(sums)
    raw = np.asarray(matrix, dtype=np.uint64)
    sexes = {r['mouse.id']: r['mouse.sex'] for r in plates.values()}
    groups = ['A' if sexes[r]=='F' else 'B' for r in ids]
    expected = statistics(raw, groups)
    actual = list(csv.DictReader(native_csv.open(encoding='utf8')))
    assert len(actual) == len(expected) == len(features)
    names = {'mean_a':'mean_log1p_a', 'mean_b':'mean_log1p_b', 'variance_a':'sample_variance_a',
             'variance_b':'sample_variance_b', 'effect':'effect_mean_log1p_b_minus_a',
             't':'welch_t', 'df':'welch_df', 'p':'p_two_sided', 'q':'q_BH'}
    errors = {key: 0.0 for key in names}
    for f, (got, wanted) in enumerate(zip(actual, expected)):
        assert got['feature_id'] == features[f] and int(got['feature_index']) == f
        assert got['status'] == wanted['status'], (f, got['status'], wanted['status'])
        for key, column in names.items():
            if wanted[key] is None:
                assert got[column] == ''
            else:
                value = float(got[column])
                np.testing.assert_allclose(value, wanted[key], rtol=2e-10, atol=2e-12)
                errors[key] = max(errors[key], abs(value-wanted[key]))
    libraries = raw.sum(axis=0)
    for r in native_replicates:
        assert int(r['raw_library_total']) == int(libraries[ids.index(r['replicate_id'])])
        assert r['sample_id'] == r['replicate_id'] and r['group_id'] == sexes[r['replicate_id']]
    result = dict(numpy=np.__version__, scipy=scipy.__version__, features=len(actual),
                  independent_replicates=ids, n_female=groups.count('A'), n_male=groups.count('B'),
                  total_counts=sum(int(x) for x in libraries), testable=sum(r['p'] is not None for r in expected),
                  max_absolute_error=errors, rtol=2e-10, atol=2e-12,
                  scope='Independent Welch/BH numerical and source association check; no biological discovery acceptance')
    output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf8')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='command', required=True)
    f = sub.add_parser('frozen'); f.add_argument('output', type=Path)
    r = sub.add_parser('prepare')
    for name in ('zip', 'metadata', 'output'): r.add_argument(name, type=Path)
    v = sub.add_parser('verify')
    for name in ('raw_csv', 'metadata', 'native_csv', 'replicates_csv', 'output'): v.add_argument(name, type=Path)
    a = p.parse_args()
    if a.command == 'frozen': frozen(a.output)
    elif a.command == 'prepare': prepare(a.zip, a.metadata, a.output)
    else: verify(a.raw_csv, a.metadata, a.native_csv, a.replicates_csv, a.output)
