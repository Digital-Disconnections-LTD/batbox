#!/usr/bin/env python3
"""Check FTXUI adapters against the pinned archive after LF and CRLF checkouts.

Uses disposable Git indexes and CMake script mode; it never configures or builds
the application. Pass --archive to use a previously downloaded release archive.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import tarfile
import tempfile
import urllib.request


def run(command, cwd, env):
    result = subprocess.run(command, cwd=cwd, env=env, capture_output=True,
                            text=True, encoding="utf-8", errors="replace", timeout=60)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout + result.stderr


def remove_readonly(function, path, _error):
    # Git object files can carry the read-only attribute on Windows.
    os.chmod(path, stat.S_IRWXU)
    function(path)


def check_checkout(source, archive, root, autocrlf, env):
    checkout = root / "checkout"
    checkout.mkdir(parents=True)
    paths = [Path(".gitattributes"), Path("cmake/FtxuiPatch.cmake")]
    patches = sorted(path.relative_to(source) for path in
                     (source / "src/terminal_ftxui/patches").glob("*.patch"))
    if not patches:
        raise RuntimeError("No FTXUI adapter patches found")
    paths += patches
    for relative in paths:
        target = checkout / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source / relative, target)
    run(["git", "init", "--quiet"], checkout, env)
    run(["git", "-c", "core.autocrlf=false", "add", "--", "."], checkout, env)
    for relative in paths:
        (checkout / relative).unlink()
    run(["git", "-c", "core.autocrlf=" + autocrlf,
         "-c", "core.eol=" + ("crlf" if autocrlf == "true" else "lf"),
         "checkout-index", "--all", "--force"], checkout, env)
    endings = {path.name: (checkout / path).read_bytes().count(b"\r\n") for path in patches}
    # FetchContent sources live inside the application's Git checkout in CI.
    # Git-style patch headers can silently skip files in this nested context.
    extracted = checkout / "build" / "_deps" / "upstream"
    with tarfile.open(archive) as package:
        package.extractall(extracted, filter="data")
    upstream = [path for path in extracted.iterdir() if path.is_dir()]
    if len(upstream) != 1:
        raise RuntimeError("Expected one FTXUI source directory in the pinned archive")
    command = ["cmake", "-Dbatbox_ftxui_upstream_SOURCE_DIR=" + upstream[0].as_posix(),
               "-P", str(checkout / "cmake/FtxuiPatch.cmake")]
    try:
        first = run(command, checkout, env)
        second = run(command, checkout, env)
    except RuntimeError as error:
        (root / "apply.log").write_text(str(error), encoding="utf-8")
        raise RuntimeError(f"autocrlf={autocrlf}; patch CRLF counts={endings}\n{error}") from error
    (root / "apply.log").write_text(first + second, encoding="utf-8")
    header = (upstream[0] / "include/ftxui/component/app.hpp").read_text(encoding="utf-8")
    implementation = (upstream[0] / "src/ftxui/component/app.cpp").read_text(encoding="utf-8")
    if ("void CommitScrollback(std::string_view rows);" not in header
            or "void App::CommitScrollback(std::string_view rows)" not in implementation):
        raise RuntimeError("Scrollback patch was skipped inside the Git checkout")
    if any(endings.values()):
        raise RuntimeError("Patch checkout changed LF bytes: " + json.dumps(endings))
    for relative in patches:
        if (checkout / relative).read_bytes() != (source / relative).read_bytes():
            raise RuntimeError("Patch content changed during checkout: " + str(relative))
    return {"autocrlf": autocrlf, "patches": len(patches), "crlfCounts": endings,
            "nestedGitCheckout": True, "scrollbackApiPresent": True,
            "freshApply": True, "repeatApply": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--keep-artifacts", action="store_true")
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    dependency = (source / "cmake/FtxuiDependency.cmake").read_text(encoding="utf-8")
    url = re.search(r"\bURL\s+(https://\S+)", dependency)
    digest = re.search(r"URL_HASH\s+SHA256=([0-9a-f]{64})", dependency)
    if not url or not digest:
        parser.error("FTXUI dependency must declare an HTTPS archive and SHA-256")
    root = Path(tempfile.mkdtemp(prefix="batbox-patch-check-")).resolve()
    env = dict(os.environ, GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=os.devnull,
               GIT_ATTR_NOSYSTEM="1")
    try:
        archive = args.archive.resolve(strict=True) if args.archive else root / "ftxui.tar.gz"
        if not args.archive:
            with urllib.request.urlopen(url.group(1), timeout=60) as response:
                archive.write_bytes(response.read())
        actual = hashlib.sha256(archive.read_bytes()).hexdigest()
        if actual != digest.group(1):
            raise RuntimeError("Pinned FTXUI archive SHA-256 mismatch")
        checks = []
        for setting in ("false", "true"):
            checks.append(check_checkout(source, archive, root / setting, setting, env))
        print(json.dumps({"passed": True, "archiveSha256": actual, "checks": checks}), flush=True)
    except Exception:
        print("Failure artifacts: " + str(root), flush=True)
        raise
    else:
        if args.keep_artifacts:
            print("Artifacts: " + str(root), flush=True)
        else:
            shutil.rmtree(root, onerror=remove_readonly)


if __name__ == "__main__":
    main()
