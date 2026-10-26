"""Generate public stubs from the installed native module.

Use the interpreter containing the current wheel, nanobind and NumPy:
  python tools/generate_stubs.py [--check]
"""
import argparse
import ast
import enum
import importlib
import re
import types
from pathlib import Path
import subprocess
import sys
import tempfile


def validate_native_signatures(text):
    for line in text.splitlines():
        if re.match(r'\s*def ',line) and re.search(r'\b(?:std|qpx)::',line):
            raise ValueError('unbound C++ type in generated stub; register the nanobind type/caster: '+line.strip())


def numpy_annotation_alias(text):
    lines=[]
    for line in text.splitlines(keepends=True):
        if line.strip()=='import numpy':line=line.replace('import numpy','import numpy as _numpy')
        elif not line.startswith(('import ','from ')):line=re.sub(r'\bnumpy\.', '_numpy.',line)
        lines.append(line)
    return ''.join(lines)


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--check',action='store_true');args=ap.parse_args()
    root=Path(__file__).resolve().parents[1];package=root/'src'/'qupathxx'
    with tempfile.TemporaryDirectory(prefix='qupathxx-stubs-') as tmp:
        raw=Path(tmp)/'_native.pyi'
        subprocess.run([sys.executable,'-m','nanobind.stubgen','-m','qupathxx._native','-o',str(raw),'-D','--exclude-values','-q'],check=True)
        text=raw.read_text(encoding='utf8').replace('from typing import Annotated, overload','from typing import Annotated, Never, overload');lines=[];current=''
        module=importlib.import_module('qupathxx._native')
        native_only={name for name,value in vars(module).items() if isinstance(value,type) and value.__module__=='qupathxx._native' and not issubclass(value,enum.Enum) and isinstance(vars(value).get('__init__'),types.WrapperDescriptorType)}
        for line in text.splitlines():
            if line.startswith('class '):current=line.split()[1].split('(')[0].rstrip(':')
            if 'def numpy(' in line and current in ('Image','PixelBuffer','FloatRgbImage'):
                dtype='float32' if current=='FloatRgbImage' else 'uint8';line=line.replace('-> object:','-> NDArray[numpy.'+dtype+']:')
            if current=='QptmaData' and 'def cores(' in line:line=line.replace('arg: list,','arg: list[QptmaCore],')
            if 'def labels_numpy(' in line:line=line.replace('-> object:','-> NDArray[numpy.uint32]:')
            if current=='PixelFeatureImage' and 'def numpy(' in line:line=line.replace('-> object:','-> NDArray[numpy.float32]:')
            if current in ('ScientificRgbGrid','ScientificChannels','ScientificRawGrid'):
                if 'def numpy(' in line:line=line.replace('-> object:','-> NDArray[numpy.float64]:')
                if 'def valid_numpy(' in line or 'def reasons_numpy(' in line:line=line.replace('-> object:','-> NDArray[numpy.uint8]:')
            if current in ('PixelFeatureImage','PixelClassification','PixelPredictionResult') and 'def valid_numpy(' in line:line=line.replace('-> object:','-> NDArray[numpy.uint8]:')
            if current in ('PixelClassification','PixelPredictionResult') and 'def vote_fractions_numpy(' in line:line=line.replace('-> object:','-> NDArray[numpy.float32]:')
            if current=='RawPixelBuffer':
                if 'def numpy(' in line:line=line.replace('-> object:', '-> NDArray[numpy.generic]:')
                if 'def valid_numpy(' in line:line=line.replace('-> object:', '-> NDArray[numpy.uint8]:')
            if current=='PixelTrainingTable':
                if 'def numpy(' in line:line=line.replace('-> object:','-> NDArray[numpy.float32]:')
                if 'def coordinates_numpy(' in line:line=line.replace('-> object:','-> NDArray[numpy.int64]:')
                if 'def annotation_ids_numpy(' in line:line=line.replace('-> object:','-> NDArray[numpy.uint64]:')
            if current=='PixelTrainingJob':
                for name in ['PixelClassifier','PixelTrainingTable']:line=line.replace('-> '+name+':','-> '+name+' | None:')
            if current=='PixelClassifierArchive':
                for name in ['PixelTrainingTable','PixelPredictionResult']:line=line.replace('-> '+name+':','-> '+name+' | None:')
            if current=='PixelContourJob':line=line.replace('-> PixelContours:', '-> PixelContours | None:')
            if current=='PixelPredictionStream' and 'def next(' in line:line=line.replace('-> PixelPredictionResult:', '-> PixelPredictionResult | None:')
            if current=='PixelPredictionJob':line=line.replace('-> PixelPredictionResult:','-> PixelPredictionResult | None:')
            if current=='ObjectFeatureTable' and 'def numpy(' in line:line=line.replace('-> object:','-> NDArray[numpy.float32]:')
            if current=='ObjectClassifierJob':
                for name in ['ForestModel','ObjectFeatureTable','ObjectClassificationResult','ObjectClassifierArchive']:
                    line=line.replace('-> '+name+':','-> '+name+' | None:')
            if current=='ObjectClassifierArchive':line=line.replace('-> ObjectClassificationResult:', '-> ObjectClassificationResult | None:')
            if current=='ObjectCellFeatureSource':line=line.replace('-> AnalysisProvenance:', '-> AnalysisProvenance | None:')
            if current in ('GuiOptions','GuiReport') and 'def pixel_' in line:
                for name in ['PixelClassifier','PixelTrainingTable','PixelPredictionResult','PixelTrainingOptions','PixelPredictionOptions','ForestParameters','PixelContours','TissueContourOptions']:
                    line=line.replace('-> '+name+':','-> '+name+' | None:').replace('arg: '+name+',','arg: '+name+' | None,')
            if current=='RawAnalysisResult' and 'def grid(' in line:
                line=line.replace('-> RawAnalysisGrid:', '-> RawAnalysisGrid | None:')
            if current=='ReaderSource' and 'def primary(' in line:
                line=line.replace('-> FileVersion:', '-> FileVersion | None:')
            if current in ('ImageServer','AnalysisProvenance') and 'def reader_source(' in line:
                line=line.replace('-> ReaderSource:', '-> ReaderSource | None:')
            if current in ('GuiOptions','GuiReport') and 'def classifier_' in line:
                for name in ['ForestModel','ObjectFeatureOptions','ForestParameters','ObjectClassificationResult']:
                    line=line.replace('-> '+name+':','-> '+name+' | None:').replace('arg: '+name+',','arg: '+name+' | None,')
            # A shared_ptr return can be empty; stubgen otherwise loses that
            # distinction for the thumbnail publication/report accessors.
            if current in ('ThumbnailJob','GuiReport'):
                line=line.replace('-> ThumbnailResult:', '-> ThumbnailResult | None:')
            lines.append(line)
            if line.startswith('class ') and current in native_only:
                lines.append('    def __init__(self, _native_only: Never) -> None: ...')
        native='# Generated by tools/generate_stubs.py; do not edit.\n'+'\n'.join(lines)+'\n'
        native=numpy_annotation_alias(native)
        validate_native_signatures(native)
    tree=ast.parse((package/'__init__.py').read_text(encoding='utf8'))
    names=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='__all__' for t in n.targets))
    names=list(dict.fromkeys(names))
    public='# Generated by tools/generate_stubs.py; do not edit.\nfrom ._native import (\n'+''.join(f'    {n} as {n},\n' for n in names)+')\n\n__all__: list[str]\n'
    for path,value in [(package/'_native.pyi',native),(package/'__init__.pyi',public),(package/'py.typed','')]:
        if args.check:
            if not path.exists() or path.read_text(encoding='utf8')!=value:raise SystemExit(f'stubs differ: {path}; regenerate against the current installed wheel')
        else:path.write_text(value,encoding='utf8')
    print('Public stubs match the installed native module' if args.check else 'Generated public stubs')


if __name__=='__main__':main()
