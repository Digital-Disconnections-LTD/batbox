#!/usr/bin/env python3
"""Package a built macOS terminal client with its exact dynamic-link inventory.

The archive uses the Homebrew libraries present at build time. It does not
rewrite install names, bundle dylibs, notarize or claim a standalone runtime.
"""
import argparse
from package_notices import stage_notices
from package_installers import stage_installers
import datetime
import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import tempfile


def digest(path: Path) -> str:
    with path.open('rb') as handle:
        return hashlib.file_digest(handle, 'sha256').hexdigest()


def linked_libraries(binary: Path) -> list[str]:
    result = subprocess.run(['otool', '-L', str(binary)], capture_output=True,
                            text=True, check=True)
    return [line.strip().split(' (', 1)[0]
            for line in result.stdout.splitlines()[1:] if line.strip()]


def architectures(binary: Path) -> list[str]:
    result = subprocess.run(['lipo', '-archs', str(binary)], capture_output=True,
                            text=True, check=True)
    return result.stdout.split()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--source', type=Path,
                        default=Path(__file__).resolve().parents[1])
    parser.add_argument('--commit', default='local-build')
    parser.add_argument('--label', default='indigenous-preview')
    args = parser.parse_args()
    if platform.system() != 'Darwin':
        parser.error('macOS packaging requires native otool and lipo')
    if not args.label or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-'
                             for c in args.label):
        parser.error('label accepts letters, digits, dots, underscores and hyphens')
    binary = args.binary.resolve(strict=True)
    source = args.source.resolve(strict=True)
    if not binary.is_file():
        parser.error('binary must be a regular file')
    for relative in ('docs/indigenous-macos.md', 'docs/BUILDING.md',
                     'third_party/ftxui/LICENSE'):
        if not (source / relative).is_file():
            parser.error(f'missing package input: {relative}')
    try:
        libraries = linked_libraries(binary)
        archs = architectures(binary)
    except (OSError, subprocess.CalledProcessError) as error:
        parser.error(f'could not inspect Mach-O binary: {error}')
    if not libraries or not archs:
        parser.error('otool/lipo did not report a linked macOS executable')
    output = args.output.resolve()
    with tempfile.TemporaryDirectory(prefix='batbox-macos-package-') as temporary:
        stage = Path(temporary) / ('batbox-macos-' + '-'.join(archs) + '-' + args.label)
        (stage / 'bin').mkdir(parents=True)
        target = stage / 'bin' / 'batbox'
        shutil.copy2(binary, target)
        target.chmod(0o755)
        shutil.copyfile(source / 'docs/indigenous-macos.md', stage / 'README.md')
        shutil.copyfile(source / 'docs/BUILDING.md', stage / 'BUILDING.md')
        (stage / 'native-client.example.json').write_text(json.dumps({
            'base_url': 'http://127.0.0.1:8000', 'model': 'your-model',
            'context_window': 200000, 'max_output_tokens': 50000,
            'compact_at': 100000, 'scrapling_command': 'scrapling',
            'searxng_url': 'http://127.0.0.1:8888', 'max_subagents': 5,
            'max_concurrent_subagents': 2,
            'permissions': {'defaultMode': 'default'}, 'fullscreen': True,
        }, indent=2) + '\n')
        (stage / 'settings.example.json').write_text(json.dumps({
            'memoryRecallEnabled': True, 'autoDreamEnabled': False,
            'timeBasedMicrocompact': False,
        }, indent=2) + '\n')
        (stage / 'licenses').mkdir()
        shutil.copyfile(source / 'third_party/ftxui/LICENSE',
                        stage / 'licenses/FTXUI-LICENSE')
        for license_file in sorted((source / 'third_party/licenses').glob('*.txt')):
            shutil.copyfile(license_file, stage / 'licenses' / license_file.name)
        stage_notices(source, stage)
        stage_installers(source, stage, 'macos')
        inventory = {'architectures': archs, 'linkedLibraries': libraries,
                     'dependencyMode': 'host-homebrew-and-macos-system'}
        (stage / 'MACHO-LINKS.json').write_text(json.dumps(inventory, indent=2) + '\n')
        record = {'kind': 'macos-homebrew-linked', 'label': args.label,
                  'commit': args.commit,
                  'builtAt': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  'sourceBinarySha256': digest(binary),
                  'packagedBinarySha256': digest(target), **inventory}
        (stage / 'BUILD.json').write_text(json.dumps(record, indent=2) + '\n')
        output.parent.mkdir(parents=True, exist_ok=True)
        staged = output.with_name(output.name + '.next')
        with tarfile.open(staged, 'w:gz') as archive:
            archive.add(stage, arcname=stage.name)
        staged.replace(output)
    print(json.dumps({'artifact': str(output), 'sha256': digest(output),
                      'architectures': archs}))


if __name__ == '__main__':
    main()
