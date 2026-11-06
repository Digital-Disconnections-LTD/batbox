#!/usr/bin/env python3
"""Bash commands run in bash or zsh: BATBOX_SHELL first, never an unsuitable SHELL.

Before, Batbox ran every Bash command with whatever $SHELL named, so a fish or
csh login shell got POSIX commands it cannot run.
"""
import os
from pathlib import Path
import shutil
import sys
import tempfile

from gadget_fixture import require, run


def executable(path, text):
    path.write_text(text, encoding="utf-8")
    path.chmod(0o755)
    return path


def main(binary):
    real_bash = shutil.which("bash") or "/bin/bash"
    with tempfile.TemporaryDirectory(prefix="batbox-shell-discovery-") as temporary:
        root = Path(temporary).resolve()
        work = root / "work"
        work.mkdir()
        tools = root / "tools"
        tools.mkdir()
        chosen = executable(tools / "my-bash", f"#!{real_bash}\nexec {real_bash} \"$@\"\n")
        marker = root / "fish-ran"
        fish = executable(tools / "fish", f"#!/bin/sh\ntouch '{marker}'\nexit 1\n")
        probe = ("call_SHELLPROBE001", "Bash", {"command": "printf 'shell:%s\\n' \"$SHELL\"",
                                                 "description": "Name the shell"})

        results, _ = run(binary, root, work, "override", [probe], ["--yolo"],
                         environment={"HOME": str(root), "BATBOX_SHELL": str(chosen),
                                      "SHELL": str(fish)})
        output = results["call_SHELLPROBE001"]
        require(f"shell:{chosen}" in output, f"BATBOX_SHELL was not used: {output!r}")

        results, _ = run(binary, root, work, "fallback", [probe], ["--yolo"],
                         environment={"HOME": str(root), "SHELL": str(fish)})
        output = results["call_SHELLPROBE001"]
        name = output.split("shell:", 1)[-1].strip()
        require("shell:" in output and os.path.basename(name) in ("bash", "zsh", "sh"),
                f"An unsuitable SHELL must give way to bash or zsh: {output!r}")
        require(not marker.exists(), "Batbox ran a Bash command with the fish SHELL")
    print("Bash commands use BATBOX_SHELL, and pass over an unsuitable SHELL")


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))
