"""Compare a Git baseline's static dependency traversal with the indexed walker.

Runs trusted repository Python source; never loads package binaries. Records all
warmups and measured pairs. Component timings do not accept hardware or releases.
"""
import argparse
import ast
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import platform
import random
import statistics
import subprocess
import time


def digest(data):
    return hashlib.sha256(data).hexdigest()


def load_module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def baseline_walker(source, require):
    # Extract the actual old loop, rather than maintaining a hand-written copy.
    tree = ast.parse(source)
    function = next(node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name == 'inspect_archive')
    start = next(i for i, node in enumerate(function.body) if isinstance(node, ast.Assign)
                 and any(isinstance(target, ast.Name) and target.id == 'closures' for target in node.targets))
    body = function.body[start:start+2]
    if not isinstance(body[1], ast.For) or ast.unparse(body[1].target) != 'root':
        raise ValueError('baseline closure extraction contract changed')
    generated = ast.parse('def walk(roots, edges, python_dll):\n    pass\n')
    generated.body[0].body = copy.deepcopy(body)+[ast.Return(value=ast.Name(id='closures', ctx=ast.Load()))]
    namespace = {'require': require}
    exec(compile(ast.fix_missing_locations(generated), '<baseline-closure-ast>', 'exec'), namespace)
    return namespace['walk'], digest(ast.dump(ast.Module(body=body, type_ignores=[])).encode('utf-8'))


def graph(count, seed):
    rng = random.Random(seed)
    names = [f'bin/module-{i:05}.dll' for i in range(count)]
    roots = ['bin/batch.exe', 'bin/region.exe', 'bin/native.pyd']
    edges = []
    def add(source, name, target, resolution):
        edges.append(dict(source=source, dependency=name, target=target, resolution=resolution))
    for root in roots:
        add(root, names[0], names[0], 'bundled-sibling')
    for i, name in enumerate(names):
        add(name, names[(i+1) % count], names[(i+1) % count], 'bundled-sibling')
        shortcut = names[rng.randrange(count)]
        add(name, shortcut, shortcut, 'bundled-sibling')
        add(name, 'kernel32.dll', None, 'windows-system')
    add(roots[-1], 'python312.dll', None, 'cpython-host-runtime')
    add('bin/unreachable.dll', 'vcruntime140.dll', None, 'msvc-runtime')
    rng.shuffle(edges)
    return roots, edges


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', required=True, help='trusted Git commit')
    parser.add_argument('--baseline-root', type=Path, required=True, help='frozen checkout of that commit')
    parser.add_argument('--gui-wheel', type=Path, required=True)
    parser.add_argument('--headless-wheel', type=Path, required=True)
    parser.add_argument('--native-archive', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--sizes', type=int, nargs='+', default=[64, 512, 4096])
    parser.add_argument('--trials', type=int, default=5)
    args = parser.parse_args()
    if not __debug__ or not 1 <= args.trials <= 10 or any(not 2 <= size <= 8192 for size in args.sizes):
        parser.error('require assertions, 1..10 trials and 2..8192 graph images')
    if args.output.exists():
        parser.error('output must be new')
    root = Path(__file__).resolve().parents[1]
    def git(*command):
        return subprocess.check_output(['git', '-C', str(root), *command])
    baseline_commit = git('rev-parse', args.baseline+'^{commit}').decode('ascii').strip()
    baseline_source = git('show', baseline_commit+':tools/ci/runtime_manifest.py')
    if (args.baseline_root/'tools/ci/runtime_manifest.py').read_bytes() != baseline_source:
        raise ValueError('baseline checkout bytes differ from requested Git source')
    for name in ['runtime_manifest.py', 'pe_dependencies.py']:
        if (args.baseline_root/'tools/ci'/name).read_bytes() != git('show', baseline_commit+':tools/ci/'+name):
            raise ValueError('baseline tooling is not frozen')
    old = load_module(args.baseline_root/'tools/ci/runtime_manifest.py', 'qpx_profile_old')
    new = load_module(root/'tools/ci/runtime_manifest.py', 'qpx_profile_new')
    walk, loop_hash = baseline_walker(baseline_source.decode('utf-8'), old.require)
    records, packages = [], []
    for label, path, gui, native in [('gui', args.gui_wheel, True, False),
                                     ('headless', args.headless_wheel, False, False),
                                     ('standalone', args.native_archive, True, True)]:
        before = old.canonical_json(old.inspect_archive(path, gui=gui, native_archive=native))
        after = new.canonical_json(new.inspect_archive(path, gui=gui, native_archive=native))
        assert before == after, label
        manifest = json.loads(after)
        packages.append(dict(label=label, archive_sha256=manifest['archive_sha256'],
                             manifest_sha256=digest(after), files=len(manifest['files']),
                             binaries=len(manifest['binaries']), identical=True))
    differential = []
    for seed in range(24):
        roots, edges = graph(2+seed*7, seed)
        assert walk(roots, edges, 'python312.dll') == new._dependency_closures(roots, edges, 'python312.dll')
        # The native executables must still reject a transitive interpreter edge.
        invalid = edges+[dict(source='bin/module-00000.dll', dependency='python312.dll',
                              target=None, resolution='cpython-host-runtime')]
        failures = []
        for implementation in [walk, new._dependency_closures]:
            try:
                implementation(roots, invalid, 'python312.dll')
            except ValueError as error:
                failures.append(str(error))
        assert failures == ['native executable transitively depends on Python']*2
        differential.append(dict(seed=seed, images=2+seed*7, valid_equal=True, invalid_equal=True))
    for size in args.sizes:
        roots, edges = graph(size, 146)
        frozen = json.dumps(edges, sort_keys=True)
        expected = walk(roots, edges, 'python312.dll')
        expected_hash = digest(new.canonical_json(expected))
        for trial in range(args.trials+1):
            order = [('baseline', walk), ('indexed', new._dependency_closures)]
            if trial % 2:
                order.reverse()
            for position, (name, implementation) in enumerate(order):
                begin = time.perf_counter_ns()
                result = implementation(roots, edges, 'python312.dll')
                elapsed = time.perf_counter_ns()-begin
                assert result == expected
                assert json.dumps(edges, sort_keys=True) == frozen
                records.append(dict(images=size, edges=len(edges), roots=len(roots), trial=trial,
                                    warmup=trial == 0, position=position, implementation=name,
                                    elapsed_ns=elapsed, output_sha256=expected_hash))
    medians = []
    for size in args.sizes:
        row = dict(images=size)
        for name in ['baseline', 'indexed']:
            row[name+'_median_ns'] = statistics.median(r['elapsed_ns'] for r in records
                if r['images'] == size and r['implementation'] == name and not r['warmup'])
        medians.append(row)
    report = dict(schema='qpx.runtime-closure-profile/1', baseline_commit=baseline_commit,
                  candidate_commit=git('rev-parse', 'HEAD').decode('ascii').strip(),
                  baseline_source_sha256=digest(baseline_source), baseline_loop_ast_sha256=loop_hash,
                  candidate_source_sha256=digest((root/'tools/ci/runtime_manifest.py').read_bytes()),
                  profiler_sha256=digest(Path(__file__).read_bytes()), python=platform.python_version(),
                  platform=platform.platform(), packages=packages, differential=differential,
                  records=records, medians=medians, release_ready=False,
                  scope='Static closure traversal only; synthetic graphs and identical real-package manifests. '
                        'No archive throughput, process memory, hardware, dynamic loader or release acceptance.')
    with args.output.open('xb') as stream:
        stream.write((json.dumps(report, indent=2)+'\n').encode('utf-8'))
    print(json.dumps(dict(packages=packages, medians=medians, observations=len(records))))


if __name__ == '__main__':
    main()
