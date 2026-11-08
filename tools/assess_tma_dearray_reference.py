"""Evaluate captured dearray observations against the preregistered synthetic grid.

A successful capture is not evidence that the detector satisfies the contract.
No tolerance is adjusted to match an observed upstream result.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path


PINNED_RUNTIME=json.loads((Path(__file__).resolve().parents[1]/'tests/fixtures/qupath-0.7.0/contract.json').read_text(encoding='utf-8'))['upstream']


def require(value,message):
    if not value:raise ValueError(message)


def number(value):
    return isinstance(value,(int,float)) and not isinstance(value,bool) and math.isfinite(value)


def assess(contract,reference):
    require(contract['schema']=='qpx.tma-dearray-fixtures/1','fixture schema differs')
    require(reference['schema']=='qpx.qupath-tma-dearray-reference/1','reference schema differs')
    require(reference['version']=='0.7.0' and reference['upstream_revision']==contract['upstream_revision'],'upstream identity differs')
    expected={c['name']:c for c in contract['cases']};observations=reference['cases']
    require(len(expected)==len(contract['cases']),'duplicate fixture case')
    require(sorted(c['name'] for c in observations)==sorted(expected),'missing/duplicate/renamed reference case')
    tolerance=contract['acceptance']['full_grid']['max_center_error_pixels']
    require(number(tolerance) and tolerance>=0,'invalid center tolerance')
    results=[]
    for observed in observations:
        case=expected[observed['name']]
        require(observed['mask_sha256']==case['mask_sha256'],'reference input mask differs')
        state=observed['state'];require(state in ['detected','not_detected','error'],'reference state differs')
        item=dict(name=case['name'],state=state,expected=case['expected'],eligible_for_automatic_acceptance=False)
        if state=='detected':
            rows,columns=observed['rows'],observed['columns']
            require(type(rows) is int and type(columns) is int and 0<rows<=512 and 0<columns<=512 and rows*columns<=10000,'invalid detected dimensions')
            points=observed['centers'];densities=observed['densities']
            require(len(points)==len(densities)==rows*columns,'detected grid payload count differs')
            require(all(len(p)==2 and all(number(v) for v in p) for p in points),'invalid detected center')
            require(all(number(v) and 0<=v<=1 for v in densities),'invalid detected density')
            exact=rows==case['rows'] and columns==case['columns']
            item.update(rows=rows,columns=columns,exact_dimensions=exact)
            if exact:
                require(len(points)==len(case['cores']),'expected slot count differs')
                errors=[math.hypot(p[0]-c['center'][0],p[1]-c['center'][1]) for p,c in zip(points,case['cores'])]
                matches=[v<contract['missing_density_threshold'] for v in densities]==[not c['present'] for c in case['cores']]
                item.update(max_center_error_pixels=max(errors),center_tolerance_passed=max(errors)<=tolerance,missing_flags_match=matches)
                item['eligible_for_automatic_acceptance']=case['expected']=='full_grid' and max(errors)<=tolerance and matches
        item['requires_review_or_refusal']=case['expected']=='review_or_refuse' or not item['eligible_for_automatic_acceptance']
        results.append(item)
    return dict(schema='qpx.tma-dearray-reference-assessment/1',center_tolerance_pixels=tolerance,
        full_grid_cases=sum(c['expected']=='full_grid' for c in results),
        within_preregistered_contract=sum(c['eligible_for_automatic_acceptance'] for c in results),
        cases=results,native_detector_implemented=False,biological_accepted=False,release_ready=False,
        scope='Synthetic binary-mask upstream characterization; not RGB preprocessing, native implementation, large-slide or biological acceptance.')


def load_and_assess(root):
    root=Path(root);raw=(root/'contract.json').read_bytes();contract=json.loads(raw)
    capture=json.loads((root/'capture-command.json').read_bytes())
    require(hashlib.sha256(raw).hexdigest()==capture['fixture_contract_sha256'],'preregistered fixture contract changed after capture')
    require(type(capture['exit']) is int and capture['exit']==0,'upstream capture did not complete')
    require(capture['runtime_contract']==PINNED_RUNTIME,'pinned runtime contract differs')
    script=Path(__file__).with_name('upstream')/'qupath_tma_dearray_reference.groovy'
    require(hashlib.sha256(script.read_bytes()).hexdigest()==capture['capture_script_sha256'],'capture script bytes differ')
    for case in contract['cases']:
        path=(root/case['mask']).resolve();require(path.is_relative_to(root.resolve()),'mask path outside fixture')
        require(hashlib.sha256(path.read_bytes()).hexdigest()==case['mask_sha256'],'fixture mask bytes differ')
    return assess(contract,json.loads((root/'qupath-reference.json').read_bytes()))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures',type=Path,default=Path(__file__).resolve().parents[1]/'tests/fixtures/tma-dearray-v1')
    parser.add_argument('--output',type=Path);args=parser.parse_args();report=load_and_assess(args.fixtures)
    text=json.dumps(report,indent=2)+'\n'
    if args.output:
        with args.output.open('x',encoding='utf-8',newline='\n') as file:file.write(text)
    print(f"{report['within_preregistered_contract']}/{report['full_grid_cases']} full-grid cases satisfy the frozen tolerance; all other observations require review or refusal")


if __name__=='__main__':main()
