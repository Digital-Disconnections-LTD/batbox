#!/usr/bin/env python3
"""Package an explicitly built native client and public setup files only."""
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
import zipfile

from package_windows import stage_windows_runtime


def digest(path):
    with path.open('rb') as handle:
        return hashlib.file_digest(handle, 'sha256').hexdigest()


def binary_target(binary):
    with binary.open('rb') as handle:
        header = handle.read(64)
        if header[:2] == b'MZ' and len(header) >= 64:
            offset = int.from_bytes(header[0x3c:0x40], 'little')
            if offset > binary.stat().st_size - 6:
                raise ValueError('Invalid PE header offset')
            handle.seek(offset)
            pe = handle.read(6)
            if pe[:4] == b'PE\0\0':
                machine = int.from_bytes(pe[4:6], 'little')
                names = {0x8664: 'x86_64', 0xaa64: 'arm64', 0x14c: 'x86'}
                if machine not in names:
                    raise ValueError(f'Unsupported Windows PE machine: {machine:#x}')
                return 'windows', names[machine]
            raise ValueError('Invalid PE signature')
        if header[:4] == b'\x7fELF':
            machine = int.from_bytes(header[18:20], 'little',
                                     signed=False) if len(header) > 19 else 0
            return 'linux', {62: 'x86_64', 183: 'arm64'}.get(machine,
                platform.machine().lower())
    raise ValueError('Unsupported binary format; use package_macos.py for Mach-O')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--commit', default='local-build')
    parser.add_argument('--label', default='halloween-preview')
    parser.add_argument('--runtime-root', type=Path,
                        help='coherent Windows UCRT64 prefix containing bin DLLs and CA data')
    parser.add_argument('--objdump',
                        help='PE objdump executable for recursive Windows DLL inventory')
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    try:
        target_os, target_arch = binary_target(binary)
    except ValueError as error:
        parser.error(str(error))
    tag = target_os + '-' + target_arch
    if target_os == 'windows' and not args.runtime_root:
        parser.error('Windows packaging requires --runtime-root for DLLs and CA data')
    if args.runtime_root and target_os != 'windows':
        parser.error('--runtime-root is only supported for Windows PE binaries')
    if not args.label or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-' for c in args.label):
        parser.error('label accepts letters, digits, dots, underscores and hyphens')
    source = args.source.resolve(strict=True)
    output = args.output.resolve()
    if target_os == 'windows' and output.suffix.lower() != '.zip':
        parser.error('Windows output must have a .zip extension')
    license_path = source / 'third_party/ftxui/LICENSE'
    if not license_path.is_file():
        parser.error('FTXUI dependency license is required in source/third_party/ftxui/LICENSE')
    with tempfile.TemporaryDirectory(prefix='batbox-package-') as temporary:
        stage = Path(temporary) / ('batbox-' + tag + '-' + args.label)
        (stage / 'bin').mkdir(parents=True)
        target = stage / 'bin' / ('batbox.exe' if target_os == 'windows' else 'batbox')
        shutil.copyfile(binary, target)
        target.chmod(0o755)
        runtime_files = []
        if target_os == 'windows':
            try:
                runtime_files = stage_windows_runtime(
                    stage, binary, args.runtime_root.resolve(strict=True), args.objdump)
            except ValueError as error:
                parser.error(str(error))
        shutil.copyfile(source / 'docs/HALLOWEEN-PREVIEW.md', stage / 'README.md')
        shutil.copyfile(source / 'docs/BUILDING.md', stage / 'BUILDING.md')
        (stage / 'licenses').mkdir(exist_ok=True)
        shutil.copyfile(license_path, stage / 'licenses/FTXUI-LICENSE')
        for dependency_license in sorted((source / 'third_party/licenses').glob('*.txt')):
            shutil.copyfile(dependency_license, stage / 'licenses' / dependency_license.name)
        stage_notices(source, stage)
        stage_installers(source, stage, target_os)
        # Fixed public templates: no user config, credentials, history or memories.
        (stage / 'native-client.example.json').write_text(json.dumps({
            'base_url': 'http://127.0.0.1:8000', 'model': 'your-model',
            'context_window': 200000, 'max_output_tokens': 50000,
            'compact_at': 100000, 'scrapling_command': 'scrapling',
            'searxng_url': 'http://127.0.0.1:8888/search', 'max_subagents': 5,
            'max_concurrent_subagents': 2,
            'permissions': {'defaultMode': 'default'}, 'fullscreen': True,
        }, indent=2) + '\n')
        (stage / 'settings.example.json').write_text(json.dumps({
            'memoryRecallEnabled': True, 'autoDreamEnabled': False,
            'timeBasedMicrocompact': False,
        }, indent=2) + '\n')
        dependencies = 'Runtime dependency inventory for this exact build.\n\n'
        if target_os == 'linux' and platform.system() == 'Linux':
            result = subprocess.run(['ldd', str(binary)], capture_output=True, text=True, check=False)
            dependencies += result.stdout + result.stderr
        elif target_os == 'windows':
            dependencies += (f'{len(runtime_files)} recursive UCRT64 DLL dependencies are in bin/.\n'
                             'The generated CA bundle is in etc/ssl/certs/.\n'
                             'Git for Windows, ripgrep and Scrapling are external helpers.\n'
                             'Optional libvips media loaders and their data are not bundled.\n')
        else:
            dependencies += 'Use the platform build guide for native runtime libraries.\n'
        dependencies += '\nGit/Bash and ripgrep support coding tools. Scrapling MCP is required at startup; see README.md. SearXNG serves web search.\n'
        (stage / 'DEPENDENCIES.txt').write_text(dependencies)
        record = {'kind': 'indigenous-preview', 'label': args.label, 'platform': tag,
                  'commit': args.commit, 'builtAt': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  'sourceBinarySha256': digest(binary), 'packagedBinarySha256': digest(target),
                  'runtimeFiles': runtime_files}
        (stage / 'BUILD.json').write_text(json.dumps(record, indent=2) + '\n')
        output.parent.mkdir(parents=True, exist_ok=True)
        staged_output = output.with_name(output.name + '.next')
        if target_os == 'windows':
            with zipfile.ZipFile(staged_output, 'w', compression=zipfile.ZIP_DEFLATED,
                                 compresslevel=9) as archive:
                for path in sorted(stage.rglob('*')):
                    if path.is_file():
                        archive.write(path, (Path(stage.name) / path.relative_to(stage)).as_posix())
        else:
            with tarfile.open(staged_output, 'w:gz') as archive:
                archive.add(stage, arcname=stage.name)
        staged_output.replace(output)
    print(json.dumps({'artifact': str(output), 'sha256': digest(output), 'platform': tag}))


if __name__ == '__main__':
    main()
