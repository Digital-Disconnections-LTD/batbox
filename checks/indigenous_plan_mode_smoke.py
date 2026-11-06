#!/usr/bin/env python3
"""A command or file change refused in plan mode must name plan mode.

Before, Write and Edit reported a directory denied by permission settings and
Bash a mode that "does not allow this action", so the model hunted for other paths.
"""
from pathlib import Path
import sys
import tempfile

from gadget_fixture import require, run


def main(binary):
    with tempfile.TemporaryDirectory(prefix="batbox-plan-mode-") as temporary:
        root = Path(temporary)
        work = root / "work"
        work.mkdir()
        steps = [
            ("call_PLANBASH00001", "Bash", {"command": "printf 'should-%s\\n' notrun",
                                            "description": "Print a line"}),
            ("call_PLANWRITE0001", "Write", {"file_path": str(work / "planned.txt"),
                                             "content": "plan mode must block this"}),
        ]
        results, _ = run(binary, root, work, "plan", steps, ["--permission-mode", "plan"])
        blocked = results["call_PLANBASH00001"]
        require("Plan mode is on" in blocked and "should-notrun" not in blocked,
                f"Plan mode must name itself when it blocks Bash: {blocked!r}")
        write = results["call_PLANWRITE0001"]
        require("Plan mode is on" in write and "denied by your permission settings" not in write,
                f"Plan mode must name itself when it blocks Write: {write!r}")
        require(not (work / "planned.txt").exists(), "Plan mode let Write create a file")
    print("Plan mode names itself when it blocks Bash and file changes")


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))
