#!/usr/bin/env python3
"""A hook with an "if" condition loads and runs only for the Gadget calls it names.

Before, any configured hook with "if" was refused when the hooks loaded.
"""
from pathlib import Path
import shlex
import sys
import tempfile

from gadget_fixture import require, run


def main(binary):
    with tempfile.TemporaryDirectory(prefix="batbox-hook-conditions-") as temporary:
        root = Path(temporary)
        work = root / "work"
        work.mkdir()
        marker = root / "hooks.txt"

        def record(label):
            return {"type": "command",
                    "command": f"printf '%s\\n' {label} >> {shlex.quote(str(marker))}"}

        hooks = {"PreToolUse": [{"matcher": "Bash", "hooks": [
            {**record("conditioned"), "if": "Bash(echo hook-*)"},
            record("always")]}]}
        steps = [
            ("call_HOOKMATCH0001", "Bash", {"command": "echo hook-yes",
                                            "description": "Print a matching line"}),
            ("call_HOOKOTHER0001", "Bash", {"command": "echo other",
                                            "description": "Print another line"}),
        ]
        results, _ = run(binary, root, work, "hooks", steps, ["--yolo"],
                         client={"hooks": hooks}, environment={"HOME": str(root)})
        require("hook-yes" in results["call_HOOKMATCH0001"],
                f"The matching command did not run: {results['call_HOOKMATCH0001']!r}")
        require("other" in results["call_HOOKOTHER0001"],
                f"The other command did not run: {results['call_HOOKOTHER0001']!r}")
        lines = marker.read_text(encoding="utf-8").split() if marker.exists() else []
        require(sorted(lines) == ["always", "always", "conditioned"],
                f"The conditioned hook must run for the matching call only: {lines!r}")
    print("A hook with an if condition runs only for the Gadget calls it names")


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))
