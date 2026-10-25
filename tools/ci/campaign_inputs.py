"""Checked inputs for complete qualification campaigns; performs no dispatch."""
import argparse
import json

INPUTS={
    'engine':dict(macos_native=False,portable_linux=False,live_reference=True,extended_readers=True),
    'macos':dict(macos_native=True,portable_linux=False,live_reference=False,extended_readers=False),
    'portable':dict(macos_native=False,portable_linux=True,live_reference=False,extended_readers=True),
}


def full_inputs(role):
    if role not in INPUTS:raise ValueError('unknown qualification role: '+role)
    return dict(INPUTS[role])


def gh_inputs(role,provided):
    expected=full_inputs(role)
    if set(provided)-set(expected) or any(type(v) is not bool for v in provided.values()):
        raise ValueError('workflow semantic inputs must use known keys and bool values')
    actual={key:provided.get(key,False) for key in expected}
    if actual!=expected:raise ValueError('incomplete qualification inputs for '+role+': expected '+json.dumps(expected,sort_keys=True))
    # gh workflow run --json requires a string-valued map, including bool inputs.
    return {key:str(value).lower() for key,value in actual.items()}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('role',choices=sorted(INPUTS));args=parser.parse_args()
    print(json.dumps(gh_inputs(args.role,full_inputs(args.role)),sort_keys=True))


if __name__=='__main__':main()
