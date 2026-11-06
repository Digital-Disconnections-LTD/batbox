#!/usr/bin/env python3
"""--debug, -d, --debug=FILTER, --debug-file and --debug-to-stderr write a debug log.

Before, each answered "Unknown option". Without them no debug log is written.
"""
from pathlib import Path
import sys
import tempfile

from gadget_fixture import require, run

STEP = [("call_DEBUGFLAGS001", "Bash", {"command": "echo debug-probe",
                                        "description": "Print a line"})]


def debug_lines(text):
    lines = [line for line in text.splitlines() if line.strip()]
    require(lines and all(" [DEBUG] " in line for line in lines),
            f"Debug log lines must carry a timestamp and [DEBUG]: {text[:600]!r}")
    return [line.split(" [DEBUG] ", 1)[1] for line in lines]


def flight(binary, root, work, name, arguments):
    completed = []
    run(binary, root, work, name, STEP, ["--yolo", *arguments],
        environment={"HOME": str(root)}, completed=completed)
    return completed[0], root / (name + "-state")


def main(binary):
    with tempfile.TemporaryDirectory(prefix="batbox-debug-flags-") as temporary:
        root = Path(temporary).resolve()
        work = root / "work"
        work.mkdir()

        _, state = flight(binary, root, work, "quiet", [])
        require(not (state / "debug").exists(), "A run without --debug wrote a debug log")

        for flag in ("--debug", "-d"):
            name = "long" if flag == "--debug" else "short"
            _, state = flight(binary, root, work, name, [flag])
            logs = sorted((state / "debug").glob("*.txt"))
            require(len(logs) == 1, f"{flag} must write one debug log: {logs}")
            lines = debug_lines(logs[0].read_text(encoding="utf-8"))
            require(any(line.startswith("Hooks:") for line in lines),
                    f"{flag} log lacks the application log lines: {lines[:10]}")
            latest = state / "debug" / "latest"
            require(latest.is_symlink() and latest.resolve() == logs[0].resolve(),
                    f"{flag}: debug/latest must point at the log")

        chosen = root / "chosen" / "debug.txt"
        _, state = flight(binary, root, work, "file", ["--debug-file", str(chosen)])
        debug_lines(chosen.read_text(encoding="utf-8"))
        require(not (state / "debug").exists(), "--debug-file also wrote the default log")

        process, state = flight(binary, root, work, "stderr", ["--debug-to-stderr"])
        debug_lines("\n".join(line for line in process.stderr.splitlines()
                              if " [DEBUG] " in line))
        require(not (state / "debug").exists(), "--debug-to-stderr also wrote a file")

        _, state = flight(binary, root, work, "filter", ["--debug=hooks"])
        lines = debug_lines(next((state / "debug").glob("*.txt")).read_text(encoding="utf-8"))
        require(all(line.startswith("Hooks:") for line in lines),
                f"--debug=hooks kept other lines: {lines[:10]}")
    print("Debug flags write a debug log; without them none is written")


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))
