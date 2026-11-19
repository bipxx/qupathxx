"""Install/use/remove the exact admitted local Loom packet.

Development orchestration only. Native CLI/SDK reports and already-qualified
reference files supply all scientific/domain/frame/identity expectations.
Only explicit, unchanged PACKAGE-owned files are eligible for removal.
"""
# SPDX-License-Identifier: AGPL-3.0-only
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
PROVIDER = 'org.qupathxx.loom'
REPORT_FIELDS = (
    'file_sha256', 'source_sha256', 'features', 'observations', 'entries',
    'domain', 'value_domain', 'input_profile',
)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def load(path):
    return json.loads(path.read_text(encoding='utf8'))


def checked_path(path):
    """Reject links before resolution, including any existing ancestor."""
    path = Path(path).absolute()
    for part in (path, *path.parents):
        try:
            info = part.lstat()
        except FileNotFoundError:
            continue
        require(not stat.S_ISLNK(info.st_mode) and not (
            getattr(info, 'st_file_attributes', 0) &
            getattr(stat, 'FILE_ATTRIBUTE_REPARSE_POINT', 0x400)),
            'Linked/reparse path is not admitted: ' + str(part))
    return path.resolve()


def member(base, name):
    pure = PurePosixPath(name)
    require(bool(name) and pure.as_posix() == name and not pure.is_absolute()
            and '..' not in pure.parts and '\\' not in name
            and ':' not in name and '\0' not in name,
            'Invalid owned member path: ' + repr(name))
    result = checked_path(base.joinpath(*pure.parts))
    require(result != base and result.is_relative_to(base),
            'Member escapes intended directory: ' + name)
    return result


def snapshot(base):
    """Return exact regular-file hashes and membership without following links."""
    base = checked_path(base)
    require(base.is_dir(), 'Missing snapshot directory: ' + str(base))
    pending, files, members = [base], {}, {}
    while pending:
        directory = pending.pop()
        for raw in sorted(directory.iterdir()):
            path = checked_path(raw)
            require(path.is_relative_to(base), 'Snapshot member escapes its root')
            name = path.relative_to(base).as_posix()
            if path.is_dir():
                members[name] = 'directory'
                pending.append(path)
            else:
                require(stat.S_ISREG(path.stat().st_mode),
                        'Special file is not an ordinary package/user member: ' + name)
                members[name] = 'file'
                files[name] = sha(path)
    return files, members


def parse_sdk_report(text):
    """Parse the SDK's native named report; do not interpret matrix samples."""
    result = {}
    for line in text.splitlines():
        key, separator, value = line.partition(' ')
        require(bool(separator) and key not in result,
                'Malformed or duplicate native SDK report field: ' + line)
        result[key] = value
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--packet', type=Path, required=True)
    parser.add_argument('--stage', type=Path)
    parser.add_argument('--admission', type=Path)
    parser.add_argument('--work', type=Path)
    parser.add_argument('--fixtures', type=Path,
                        help='Original passing fixture directory or native-reference.json')
    parser.add_argument('--cache-binaries', type=Path, required=True)
    args = parser.parse_args()

    workspace = checked_path(ROOT)
    packet = checked_path(args.packet)
    stage = checked_path(args.stage or packet / 'package-final')
    admission_path = checked_path(args.admission or packet / 'package-final-admission.json')
    work = checked_path(args.work or packet / 'deployment-01')
    fixture_argument = checked_path(args.fixtures or packet / 'integration-03/fixtures')
    reference_path = (fixture_argument if fixture_argument.is_file()
                      else fixture_argument / 'native-reference.json')
    reference_path = checked_path(reference_path)
    fixtures = reference_path.parent
    cache = checked_path(args.cache_binaries)
    installation = checked_path(work / 'installation')

    require(packet != workspace and packet.is_relative_to(workspace / 'build'),
            'Packet must be a named project build directory')
    for name, path in [('stage', stage), ('admission', admission_path),
                       ('work', work), ('fixtures', fixtures)]:
        require(path != packet and path.is_relative_to(packet),
                name + ' must stay inside the named packet')
    require(installation.is_relative_to(work) and installation.is_relative_to(packet)
            and installation.is_relative_to(workspace) and installation != work,
            'Installation target escapes the packet/workspace')
    require(not work.exists(), 'Use a fresh work directory; preserve earlier evidence')
    require(not stage.is_relative_to(work) and not work.is_relative_to(stage)
            and not fixtures.is_relative_to(work) and not work.is_relative_to(fixtures)
            and not cache.is_relative_to(work),
            'Installation work must not overlap staged, original fixture or cached products')
    require(stage.is_dir() and reference_path.is_file() and admission_path.is_file(),
            'Complete the admitted stage and passing fixtures before this check')
    require(cache.is_dir() and cache.is_relative_to(workspace),
            'Cache binaries must be retained workspace products')
    require(os.name == 'nt', 'This local delivery check claims the Windows profile only')
    work.mkdir(parents=True, exist_ok=False)
    result = {
        'schema': 'qpx.loom-deployment/1', 'passed': False, 'goal_complete': False,
        'scope': 'Exact installed native CLI/SDK interoperability and owned-file removal; no new numerical/platform qualification',
        'stage': str(stage), 'installation': str(installation),
        'controller_sha256': sha(Path(__file__)), 'checks': {},
    }

    def save():
        (work / 'results.json').write_text(
            json.dumps(result, indent=2) + '\n', encoding='utf8')

    windows = checked_path(Path(os.environ['SystemRoot']))
    env = {key: value for key, value in os.environ.items()
           if not key.upper().startswith(('PYTHON', 'VIRTUAL_ENV', 'VENV', 'CONDA'))}
    env['PATH'] = os.pathsep.join(map(str, (windows / 'System32', windows)))
    result['native_environment'] = {
        'PATH': env['PATH'], 'python_venv_conda_variables_removed': True,
        'runtime_dependencies': 'Exact executable/provider/runtime DLLs co-located in installed bin',
    }
    save()

    def run(name, command, expected=0, json_output=False):
        require(name not in result['checks'], 'Duplicate command receipt: ' + name)
        directory = work / 'commands' / name
        directory.mkdir(parents=True, exist_ok=False)
        argv = list(map(str, command))
        process = subprocess.run(
            argv, cwd=directory, env=env, capture_output=True,
            encoding='utf8', errors='strict', timeout=180)
        row = {
            'argv': argv, 'returncode': process.returncode,
            'stdout': process.stdout, 'stderr': process.stderr,
            'native_only_environment': True, 'process_exited': True,
        }
        result['checks'][name] = row
        (directory / 'stdout.log').write_text(process.stdout, encoding='utf8')
        (directory / 'stderr.log').write_text(process.stderr, encoding='utf8')
        save()
        require(process.returncode == expected,
                name + ': unexpected exit ' + str(process.returncode) + '; see ' + str(directory))
        print(name + ': passed', flush=True)
        return json.loads(process.stdout) if json_output else row

    try:
        admission = load(admission_path)
        ownership = load(stage / 'PACKAGE.json')
        require(admission.get('admission_exit') == 0,
                'Package material admission has not passed')
        names = set(ownership['owned_files'])
        require(len(names) == len(ownership['owned_files']) and bool(names)
                and names == set(admission['members']),
                'Ownership/admission member sets differ or contain duplicates')
        stage_files, _ = snapshot(stage)
        require(names == set(stage_files), 'Stage contains missing or unowned members')
        expected_owned = {}
        for name in sorted(names):
            path = member(stage, name)
            record = admission['members'][name]
            require(path.stat().st_size == record['bytes']
                    and stage_files[name] == record['sha256'],
                    'Changed admitted staged member: ' + name)
            expected_owned[name] = {
                'bytes': record['bytes'], 'sha256': record['sha256'],
                'material_id': record['material_id'],
            }
        result['package'] = {
            'owned_members': expected_owned,
            'package_json_sha256': sha(stage / 'PACKAGE.json'),
            'admission_path': str(admission_path), 'admission_sha256': sha(admission_path),
        }
        reference = load(reference_path)
        require(reference.get('schema') == 'qpx.loom-native-reference/1',
                'Unexpected native reference schema')
        input_paths = [reference[name] for name in ('image', 'input_project', 'positions')]
        input_paths += [reference[kind][field] for kind in ('counts', 'continuous')
                        for field in ('input', 'path')]
        for name in input_paths:
            path = checked_path(Path(name))
            require(path.is_relative_to(fixtures) and path.is_file(),
                    'Use original native fixture paths, not relocated copies: ' + name)
        original_files, original_members = snapshot(fixtures)
        result['native_reference'] = {
            'path': str(reference_path), 'sha256': sha(reference_path),
            'frame_sha256': reference['frame_sha256'],
            'input_project': reference['input_project'], 'image_id': reference['image_id'],
            'counts': reference['counts'], 'continuous': reference['continuous'],
        }
        result['protected_inputs'] = {
            str(fixtures / name): digest for name, digest in original_files.items()}
        result['original_source_managed_files'] = {
            name: digest for name, digest in original_files.items()
            if name.startswith('.qpx-expression/')}
        for kind in ('counts', 'continuous'):
            ref = reference[kind]
            require(sha(Path(ref['path'])) == ref['file_sha256'],
                    'Changed native QMX reference: ' + kind)
            source_copy = fixtures / '.qpx-expression' / (ref['file_sha256'] + '.qmx')
            require(source_copy.is_file() and sha(source_copy) == ref['file_sha256'],
                    'Required already-retained source-side managed copy is absent: ' + kind)

        def unchanged_inputs():
            current_files, current_members = snapshot(fixtures)
            require(current_files == original_files and current_members == original_members,
                    'Original fixture/project/source-side managed files changed')

        installation.mkdir()
        for name, record in expected_owned.items():
            source, target = member(stage, name), member(installation, name)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
            require(target.stat().st_size == record['bytes'] and sha(target) == record['sha256'],
                    'Installed member differs from admitted stage: ' + name)
        result['installed_members'] = len(names)
        notes = ('user-notes.txt', 'bin/user-tool-notes.txt', 'lib/user-sdk-notes.txt')
        for name in notes:
            target = member(installation, name)
            require(name not in names and not target.exists(), 'User note collides with package')
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text('Preserve owner-created file: ' + name + '\n', encoding='utf8')
        user_results = member(installation, 'user-results')
        user_results.mkdir()
        binaries = installation / 'bin'
        manifest = binaries / 'loom-format-plugins.json'
        cli = binaries / 'qupathxx_format_import.exe'
        consumer = binaries / 'loom_matrix_consumer.exe'
        for path in (manifest, cli, consumer):
            require(path.relative_to(installation).as_posix() in names,
                    'Required installed entry point is not package-owned: ' + str(path))
        cache_cli = checked_path(cache / cli.name)
        require(cache_cli.is_file() and sha(cache_cli) == expected_owned['bin/' + cli.name]['sha256'],
                'Post-removal CLI must match the qualified installed CLI bytes')
        catalog = run('installed-catalog', [cli, '--format-plugins', manifest, 'catalog'],
                      json_output=True)
        native_receipt = json.loads(reference['counts']['reader_receipt_json'])
        require(catalog.get('schema') == 'qpx.format-plugin-catalog/1'
                and catalog['diagnostics'] == [] and len(catalog['providers']) == 1,
                'Installed provider catalog/admission differs')
        selected = catalog['providers'][0]
        require(selected['provider_id'] == PROVIDER and selected['domain'] == 'matrix'
                and selected['abi'] == 2
                and selected['module_sha256'] == native_receipt['provider']['module_sha256']
                and selected['implementation_sha256'] == native_receipt['implementation_sha256'],
                'Installed provider identity differs from the passing native source')

        def report_matches(report, ref, state, command):
            require(report.get('schema') == 'qpx.format-import-cli/1'
                    and report.get('state') == state and report.get('command') == command,
                    'Unexpected native CLI result schema/state/command')
            for field in REPORT_FIELDS:
                require(report[field] == ref[field], 'Native CLI reference differs: ' + field)
            require(report['frame_sha256'] == reference['frame_sha256']
                    and report['receipt'] == json.loads(ref['reader_receipt_json'])
                    and report['positions_sha256'] == ref['positions_sha256']
                    and report['position_columns'] == ['observation_id', 'x_px', 'y_px'],
                    'Native CLI frame/source/reader/position receipt differs')

        outputs = {}
        for kind in ('counts', 'continuous'):
            ref = reference[kind]
            cli_qmx = user_results / ('cli-' + kind + '.qmx')
            command = [cli, '--format-plugins', manifest, 'matrix-index', PROVIDER,
                       ref['input'], reference['positions'], reference['image'], cli_qmx,
                       '--plugin-options-json', ref['options_json']]
            indexed = run('installed-cli-' + kind, command, json_output=True)
            report_matches(indexed, ref, 'indexed', 'matrix-index')
            require(cli_qmx.read_bytes() == Path(ref['path']).read_bytes()
                    and sha(cli_qmx) == ref['file_sha256'],
                    'Installed CLI QMX bytes differ from native reference: ' + kind)
            info = run('installed-info-without-provider-' + kind,
                       [cli, 'matrix-info', cli_qmx], json_output=True)
            report_matches(info, ref, 'metadata', 'matrix-info')
            existing = run('installed-existing-output-' + kind, command, expected=2)
            existing_error = json.loads(existing['stderr'])
            require(existing_error.get('schema') == 'qpx.format-import-error/1'
                    and 'exist' in existing_error.get('error', '').lower()
                    and sha(cli_qmx) == ref['file_sha256'],
                    'Existing-output route missed its native new-only refusal')
            sdk_qmx = user_results / ('sdk-' + kind + '.qmx')
            sdk_project = user_results / ('sdk-' + kind + '.qpx')
            sdk_row = run('installed-sdk-' + kind, [
                consumer, manifest, reference['input_project'], reference['image_id'],
                ref['input'], reference['positions'], sdk_qmx,
                ref['options_json'], sdk_project])
            sdk = parse_sdk_report(sdk_row['stdout'])
            for field in ('features', 'observations', 'entries', 'file_sha256',
                          'source_sha256', 'value_domain', 'input_profile', 'positions_sha256'):
                require(sdk[field] == str(ref[field]), 'SDK reference differs: ' + field)
            require(sdk['image'] == str(reference['image_id']) and sdk['value_kind'] == kind
                    and sdk['frame_sha256'] == reference['frame_sha256']
                    and sdk['receipt_sha256'] == ref['reader_receipt_sha256']
                    and json.loads(sdk['receipt']) == json.loads(ref['reader_receipt_json'])
                    and sdk['owned_file_sha256'] == ref['file_sha256']
                    and sdk['owned_source_sha256'] == ref['source_sha256']
                    and bool(sdk['association_id'])
                    and checked_path(Path(sdk['project'])) == checked_path(sdk_project),
                    'SDK owned association/frame/receipt identity differs')
            managed_qmx = member(user_results, '.qpx-expression/' + ref['file_sha256'] + '.qmx')
            require(sdk_project.is_file()
                    and sdk_qmx.read_bytes() == Path(ref['path']).read_bytes()
                    and sha(sdk_qmx) == ref['file_sha256']
                    and managed_qmx.read_bytes() == Path(ref['path']).read_bytes(),
                    'SDK project/owned QMX did not preserve exact native bytes')
            outputs[kind] = {
                'cli_qmx': str(cli_qmx), 'sdk_qmx': str(sdk_qmx),
                'sdk_project': str(sdk_project), 'managed_qmx': str(managed_qmx),
                'sdk_project_sha256': sha(sdk_project),
                'file_sha256': ref['file_sha256'], 'source_sha256': ref['source_sha256'],
                'association_id': sdk['association_id'],
                'reader_receipt_sha256': ref['reader_receipt_sha256'],
            }
            unchanged_inputs()
        result['outputs'] = outputs
        result['installed_native_checks'] = len(result['checks'])
        save()

        # Every spawned native process has returned. Check the fresh installation
        # independently for active executable users before considering removal.
        checked_path(installation)
        require(installation.is_relative_to(packet) and installation.is_relative_to(workspace)
                and installation != packet and installation != workspace,
                'Removal target escaped the exact intended installation')
        query = (
            "$ErrorActionPreference='Stop'; "
            "$busy=@(Get-CimInstance Win32_Process | Where-Object { "
            "$_.ExecutablePath -and $_.ExecutablePath.StartsWith("
            "$env:QPX_LOOM_INSTALL + '\\',[System.StringComparison]::OrdinalIgnoreCase) "
            "} | Select-Object ProcessId,Name,ExecutablePath); "
            "ConvertTo-Json -InputObject $busy -Compress"
        )
        process_env = dict(env, QPX_LOOM_INSTALL=str(installation))
        powershell = windows / 'System32/WindowsPowerShell/v1.0/powershell.exe'
        process = subprocess.run(
            [str(powershell), '-NoProfile', '-NonInteractive', '-Command', query],
            cwd=work, env=process_env, capture_output=True,
            encoding='utf8', errors='strict', timeout=45)
        process_check = {
            'argv': [str(powershell), '-NoProfile', '-NonInteractive', '-Command', query],
            'installation_environment_value': str(installation),
            'returncode': process.returncode, 'stdout': process.stdout,
            'stderr': process.stderr, 'all_spawned_native_processes_exited': True,
        }
        result['process_check'] = process_check
        (work / 'process-check.stdout.log').write_text(process.stdout, encoding='utf8')
        (work / 'process-check.stderr.log').write_text(process.stderr, encoding='utf8')
        save()
        require(process.returncode == 0 and not process.stderr.strip()
                and json.loads(process.stdout) == [],
                'Active installation users could not be ruled out; no files removed')

        all_files, _ = snapshot(installation)
        unowned = {name: digest for name, digest in all_files.items() if name not in names}
        require(all(name in unowned for name in notes), 'User preservation probes are missing')
        for output in outputs.values():
            for field in ('cli_qmx', 'sdk_qmx', 'sdk_project', 'managed_qmx'):
                require(Path(output[field]).relative_to(installation).as_posix() in unowned,
                        'Research output was incorrectly classified as package-owned')
        unchanged_inputs()
        # Validate the entire deletion set first. Unlink only these explicit
        # unchanged package paths; never recurse into a user or build directory.
        deletion = []
        for name, record in expected_owned.items():
            target = member(installation, name)
            require(target.is_file() and target.stat().st_size == record['bytes']
                    and sha(target) == record['sha256'],
                    'Changed/missing owned file prevents all removal: ' + name)
            deletion.append(target)
        result['removal_preflight'] = {
            'resolved_target': str(installation), 'owned_members_verified': len(deletion),
            'unowned_files_to_preserve': unowned, 'original_inputs_unchanged': True,
        }
        save()
        for target in deletion:
            target.unlink()
        parents = {
            parent for target in deletion for parent in target.parents
            if parent != installation and parent.is_relative_to(installation)}
        removed_directories = []
        for directory in sorted(parents, key=lambda path: len(path.parts), reverse=True):
            checked_path(directory)
            require(directory.is_relative_to(installation), 'Owned parent escaped installation')
            if directory.exists() and not any(directory.iterdir()):
                directory.rmdir()
                removed_directories.append(directory.relative_to(installation).as_posix())
        remaining, _ = snapshot(installation)
        require(remaining == unowned
                and all(not member(installation, name).exists() for name in names),
                'Owned removal changed or removed an unowned user file')

        for kind in ('counts', 'continuous'):
            info = run('retained-owned-matrix-info-' + kind, [
                cache_cli, 'matrix-info', outputs[kind]['managed_qmx']], json_output=True)
            report_matches(info, reference[kind], 'metadata', 'matrix-info')
        ref = reference['counts']
        unavailable_output = user_results / 'missing-provider.qmx'
        unavailable = run('retained-missing-provider-import', [
            cache_cli, 'matrix-index', PROVIDER, ref['input'], reference['positions'],
            reference['image'], unavailable_output,
            '--plugin-options-json', ref['options_json']], expected=2)
        unavailable_error = json.loads(unavailable['stderr'])
        require(unavailable_error.get('schema') == 'qpx.format-import-error/1'
                and PROVIDER in unavailable_error.get('error', '')
                and not unavailable_output.exists(),
                'Valid unavailable-provider request did not reach its native refusal')
        final_remaining, _ = snapshot(installation)
        require(final_remaining == unowned, 'Post-removal inspection changed preserved user files')
        unchanged_inputs()
        result['removal'] = {
            'owned_removed': len(deletion), 'empty_owned_parents_removed': removed_directories,
            'unowned_preserved': final_remaining, 'retained_user_files': len(final_remaining),
            'retained_native_owned_qmx_metadata': True, 'missing_provider_import_refused': True,
            'original_input_files_unchanged': len(original_files),
            'original_source_managed_files_preserved': True,
            'separate_sdk_core_source_products_removed': False,
        }
        result['passed'] = True
        save()
        print(json.dumps({
            'passed': True, 'installed_and_removed': len(names),
            'preserved_user_files': len(final_remaining),
            'installed_native_checks': result['installed_native_checks'],
            'total_native_checks': len(result['checks']), 'goal_complete': False,
        }))
    except BaseException as error:
        result['failure'] = {'type': type(error).__name__, 'message': str(error)}
        save()
        raise


if __name__ == '__main__':
    main()
