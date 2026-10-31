"""Stage the Windows UCRT runtime next to a native Batbox executable.

This module deliberately reads PE imports from the binary being packaged.  A
runtime assembled for an older build is useful only if it satisfies this exact
binary's import graph.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess


_IMPORT = re.compile(r"^\s*DLL Name:\s*(\S+)\s*$", re.IGNORECASE | re.MULTILINE)
_SYSTEM_DLLS = {
    "advapi32.dll", "bcrypt.dll", "bcryptprimitives.dll", "cfgmgr32.dll",
    "comctl32.dll", "comdlg32.dll", "crypt32.dll", "d3d9.dll", "dbghelp.dll",
    "dnsapi.dll", "dwrite.dll", "dxgi.dll", "gdi32.dll", "gdiplus.dll",
    "imm32.dll", "iphlpapi.dll", "kernel32.dll", "msimg32.dll",
    "msvcrt.dll", "ntdll.dll", "ole32.dll", "oleaut32.dll", "psapi.dll",
    "rpcrt4.dll", "secur32.dll", "setupapi.dll", "shell32.dll",
    "shlwapi.dll", "user32.dll", "userenv.dll", "usp10.dll",
    "version.dll", "winmm.dll", "wldap32.dll", "ws2_32.dll", "xmllite.dll",
}
_NOTICE_NAMES = ("license", "licence", "copying", "copyright", "notice")
_COMMON_REFERENCE = re.compile(r"/(?:usr/)?share/licenses/common/([A-Za-z0-9_+./-]+)")


def _sha256(path: Path) -> str:
    with path.open("rb") as handle:
        return hashlib.file_digest(handle, "sha256").hexdigest()


def _objdump_imports(path: Path, objdump: str) -> list[str]:
    result = subprocess.run([objdump, "-p", str(path)], capture_output=True,
                            text=True, errors="replace", check=False)
    if result.returncode:
        raise ValueError(f"objdump failed for {path.name}: {result.stderr.strip()}")
    return _IMPORT.findall(result.stdout)


def _runtime_dlls(runtime_bin: Path) -> dict[str, Path]:
    if not runtime_bin.is_dir():
        raise ValueError(f"Windows runtime bin directory is missing: {runtime_bin}")
    result: dict[str, Path] = {}
    for path in runtime_bin.iterdir():
        if path.is_file() and path.suffix.casefold() == ".dll":
            name = path.name.casefold()
            if name in result:
                raise ValueError(f"Ambiguous Windows DLL name: {path.name}")
            result[name] = path
    return result


def stage_windows_notices(stage: Path, runtime_root: Path) -> None:
    """Preserve installed distribution records without guessing DLL ownership."""
    runtime_root = runtime_root.resolve(strict=True)
    license_root = runtime_root / "share/licenses"
    if not license_root.is_dir():
        raise ValueError("Windows runtime requires installed share/licenses records")
    records: dict[str, Path] = {}

    def collect(directory: Path, prefix: str, only_notices: bool = False) -> None:
        if not directory.is_dir():
            return
        boundary = directory.resolve(strict=True)
        for path in sorted(directory.rglob("*")):
            if path.is_dir():
                continue
            if only_notices and not path.name.lower().startswith(_NOTICE_NAMES):
                continue
            if not path.is_file() or not path.resolve().is_relative_to(boundary):
                raise ValueError(f"Invalid Windows runtime notice record: {path}")
            if not path.stat().st_size:
                raise ValueError(f"Empty Windows runtime notice record: {path}")
            records[f"{prefix}/{path.relative_to(directory).as_posix()}"] = path

    collect(license_root, "prefix/share/licenses")
    if not records:
        raise ValueError("Windows runtime share/licenses contains no notice records")
    collect(runtime_root / "share/doc", "prefix/share/doc", only_notices=True)
    collect(runtime_root / "share/icu", "prefix/share/icu", only_notices=True)
    common = runtime_root.parent / "usr/share/licenses/common"
    collect(common, "msys/share/licenses/common")

    # Some distributions reference shared texts instead of duplicating them.
    # Preserve both supported common trees, and reject dangling references.
    common_roots = (license_root / "common", common)
    for path in records.values():
        for reference in _COMMON_REFERENCE.findall(path.read_text(errors="replace")):
            relative = Path(reference.rstrip("."))
            if relative.is_absolute() or ".." in relative.parts or not any(
                    (directory / relative).is_file() for directory in common_roots):
                raise ValueError(f"Missing referenced Windows common license: {reference}")

    destination = stage / "licenses/windows-runtime"
    inventory = []
    for relative, source in sorted(records.items()):
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        inventory.append({"file": relative, "sha256": _sha256(target),
                          "bytes": target.stat().st_size})
    (destination / "INVENTORY.json").write_text(json.dumps({
        "scope": "Installed distribution notice records; may include unbundled packages.",
        "coverage": "Review against BUILD.json runtimeFiles and exact package metadata. "
                    "This inventory does not establish per-DLL license coverage or "
                    "fulfill source/relinking requirements.",
        "records": inventory,
    }, indent=2) + "\n")


def stage_windows_runtime(stage: Path, binary: Path, runtime_root: Path,
                          objdump: str | None = None) -> list[dict[str, object]]:
    """Copy transitive PE imports, CA data, and installed distribution notices.

    ``runtime_root`` is a coherent UCRT64 prefix (or a staged root) containing
    ``bin/*.dll`` and ``etc/ssl/certs/ca-bundle.crt``.  The caller has already
    copied ``binary`` to ``stage/bin/batbox.exe``.
    """
    if objdump is None:
        objdump = (shutil.which("x86_64-w64-mingw32-objdump") or
                   shutil.which("objdump") or shutil.which("llvm-objdump"))
    if not objdump:
        raise ValueError("PE objdump is required (--objdump or a tool on PATH)")
    runtime_bin = runtime_root / "bin"
    available = _runtime_dlls(runtime_bin)
    target_bin = stage / "bin"
    target_bin.mkdir(parents=True, exist_ok=True)
    queue = [binary]
    inspected: set[str] = set()
    copied: dict[str, Path] = {}
    while queue:
        current = queue.pop(0)
        key = str(current.resolve()).casefold()
        if key in inspected:
            continue
        inspected.add(key)
        for name in _objdump_imports(current, objdump):
            folded = name.casefold()
            if folded.startswith(("api-ms-win-", "ext-ms-win-")) or folded in _SYSTEM_DLLS:
                continue
            if folded not in available:
                raise ValueError(f"Missing Windows runtime DLL {name} imported by {current.name}")
            if folded not in copied:
                source = available[folded]
                destination = target_bin / source.name
                shutil.copyfile(source, destination)
                copied[folded] = source
                queue.append(source)
    certificate = runtime_root / "etc/ssl/certs/ca-bundle.crt"
    if not certificate.is_file() or b"-----BEGIN CERTIFICATE-----" not in certificate.read_bytes():
        raise ValueError("Windows runtime requires a populated etc/ssl/certs/ca-bundle.crt")
    target_ca = stage / "etc/ssl/certs"
    target_ca.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(certificate, target_ca / certificate.name)
    trust = runtime_root / "etc/ssl/certs/ca-bundle.trust.crt"
    if trust.is_file() and trust.stat().st_size:
        shutil.copyfile(trust, target_ca / trust.name)
    stage_windows_notices(stage, runtime_root)
    return [{"file": f"bin/{source.name}", "sha256": _sha256(source),
             "bytes": source.stat().st_size} for source in
            sorted(copied.values(), key=lambda item: item.name.casefold())]
