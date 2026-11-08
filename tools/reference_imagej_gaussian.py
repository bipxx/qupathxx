"""Generate native fixture metadata from the authenticated public stage capture."""
import argparse
import importlib.util
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
TARGET=ROOT/'cpp/tests/imagej_gaussian_reference.hpp'


def generate():
    spec=importlib.util.spec_from_file_location('gaussian_native_capture',ROOT/'tools/qupath_gaussian.py')
    reference=importlib.util.module_from_spec(spec);spec.loader.exec_module(reference)
    reference.verify();cases,_=reference.inputs();manifest=reference.read(reference.FIXTURE/'manifest.json')
    lines=['#pragma once','#include <cstddef>','namespace imagej_gaussian_reference {',
        'struct Case {const char* name;std::size_t width,height;double sigma;const char* input_hash;const char* blur_hash;const char* log_hash;};',
        'inline constexpr Case cases[]={']
    for case in cases:
        hashes=[manifest['files'][case['name']+'-'+kind+'.f32le']['sha256'] for kind in ('input','blur','log')]
        lines.append('    {"'+case['name']+'",'+str(case['width'])+','+str(case['height'])+','+repr(case['sigma'])+','+','.join('"'+h+'"' for h in hashes)+'},')
    return '\n'.join([*lines,'};','}',''])


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true');args=parser.parse_args()
    expected=generate().encode()
    if args.check:
        if TARGET.read_bytes()!=expected:raise ValueError('native Gaussian metadata differs from authenticated capture')
    else:TARGET.write_bytes(expected)


if __name__=='__main__':main()
