#!/usr/bin/env python3
"""Grep and Glob errors must read as errors, and partial results must say so.

A bad regex is a failed search, not "No matches found". Results cut short by an
unreadable path or ripgrep's output limit say why. A Glob cut at its limit keeps
the newest files.
"""
import os
from pathlib import Path
import sys
import tempfile

from gadget_fixture import require, run


UNREADABLE = os.name != "nt" and os.geteuid() != 0


def steps(work):
    found = [
        ("call_BADREGEX00001", "Grep", {"pattern": "unclosed(", "path": str(work / "grep")}),
        ("call_OVERFLOW00001", "Grep", {"pattern": "needle", "path": str(work / "big"),
                                        "output_mode": "content"}),
        ("call_GLOBNEWEST001", "Glob", {"pattern": "*.glb", "path": str(work / "globs")}),
    ]
    if UNREADABLE:
        found.append(("call_PATHERROR0001", "Grep", {"pattern": "findme",
                                                     "path": str(work / "grep")}))
    return found


def prepare(work):
    (work / "grep").mkdir()
    (work / "grep" / "found.txt").write_text("findme here\n", encoding="utf-8")
    if UNREADABLE:
        locked = work / "grep" / "locked"
        locked.mkdir()
        (locked / "inside.txt").write_text("findme too\n", encoding="utf-8")
        locked.chmod(0)
    (work / "big").mkdir()
    # About 22 MB of matching lines: ripgrep's content output passes its 20 MB buffer.
    line = "needle " + "x" * 33 + "\n"
    with open(work / "big" / "haystack.txt", "w", encoding="utf-8") as handle:
        for _ in range(16):
            handle.write(line * 35000)
    globs = work / "globs"
    globs.mkdir()
    for index in range(105):
        path = globs / f"f{index:03d}.glb"
        path.write_text("x", encoding="utf-8")
        os.utime(path, (1_600_000_000 + index * 60, 1_600_000_000 + index * 60))


def main(binary):
    with tempfile.TemporaryDirectory(prefix="batbox-search-results-") as temporary:
        root = Path(temporary)
        work = root / "work"
        work.mkdir()
        prepare(work)
        try:
            results, _ = run(binary, root, work, "search", steps(work), ["--yolo"])
        finally:
            if UNREADABLE:
                (work / "grep" / "locked").chmod(0o755)

        regex = results["call_BADREGEX00001"]
        require("regex parse error" in regex and "No matches found" not in regex
                and "No files found" not in regex, f"A bad regex must read as an error: {regex!r}")

        overflow = results["call_OVERFLOW00001"]
        require("Results are incomplete" in overflow and "MB limit" in overflow,
                f"Results cut at the output limit must say so: {overflow[:600]!r}")

        if UNREADABLE:
            partial = results["call_PATHERROR0001"]
            require("found.txt" in partial and "Some paths could not be searched" in partial
                    and "locked" in partial,
                    f"Matches beside an unreadable directory must name it: {partial!r}")

        globbed = results["call_GLOBNEWEST001"]
        require("f104.glb" in globbed and "f000.glb" not in globbed and "truncated" in globbed,
                f"A truncated Glob must keep the newest files: {globbed!r}")
    print("Search errors read as errors, partial results say why, and Glob keeps the newest files")


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))
