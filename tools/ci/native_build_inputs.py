"""Shared development-only MSBuild input/command capture, without building."""
import hashlib
from pathlib import Path


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def msvc_inputs(root, cache, selected):
    """Read exact selected target folders, including source markers and headers."""
    root, cache = Path(root).resolve(), Path(cache).resolve()
    sources, groups, read_logs, commands = {}, {}, {}, {}
    for directory in cache.rglob('*.dir'):
        target = directory.stem
        if not selected(target):
            continue
        logs = list(directory.glob('Release/*.tlog/CL.read.1.tlog'))
        if not logs:
            continue
        assert target not in groups, 'Ambiguous selected target folder: ' + target
        files = set()
        for log in logs:
            for line in log.read_text(encoding='utf16').splitlines():
                for value in line[1:].split('|') if line.startswith('^') else [line]:
                    if not value:
                        continue
                    path = Path(value)
                    if path.is_file():
                        path = path.resolve()
                        if path.is_relative_to(root):
                            relative = path.relative_to(root).as_posix()
                            sources[relative] = sha(path)
                            files.add(relative)
            command = log.with_name('CL.command.1.tlog')
            if command.is_file():
                commands[target] = {'path': command.relative_to(root).as_posix(),
                                    'sha256': sha(command),
                                    'text': command.read_text(encoding='utf16')}
        groups[target] = sorted(files)
        read_logs[target] = [p.relative_to(root).as_posix() for p in logs]
    return sources, groups, read_logs, commands
