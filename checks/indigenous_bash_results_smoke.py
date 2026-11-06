#!/usr/bin/env python3
"""Bash results must hand the model everything the command produced.

A failing build's summary is at the end of its log, so a large output must show
its end and the saved file, on failure as on success. A command Batbox ended
must say why. Expected text is built so it cannot appear in the command itself.
"""
from pathlib import Path
import re
import sys
import tempfile

from gadget_fixture import require, run


FILLER = ("i=0; while [ $i -lt 4000 ]; do printf 'filler line %05d abcdefghijklmnop\\n' $i; "
          "i=$((i+1)); done; ")


def bash(call_id, command, **extra):
    return (call_id, "Bash", dict({"command": command, "description": "Fixture command"}, **extra))


STEPS = [
    bash("call_LARGEFAIL0001", "printf 'head-%s\\n' marker; " + FILLER +
         "printf 'tail-%s\\n' summary; exit 2"),
    bash("call_LARGEPASS0001", "printf 'start-%s\\n' marker; " + FILLER +
         "printf 'end-%s\\n' summary"),
    bash("call_SLEEPTIME0001", "sleep 30", timeout=1500),
    ("call_UNKNOWNFLD001", "Bash", {"command": "pwd", "cwd": "/"}),
]


def main(binary):
    with tempfile.TemporaryDirectory(prefix="batbox-bash-results-") as temporary:
        root = Path(temporary)
        work = root / "work"
        work.mkdir()
        results, tools = run(binary, root, work, "bash", STEPS, ["--yolo"])

        failed = results["call_LARGEFAIL0001"]
        require("Exit code 2" in failed, f"Exit code missing from the large failure: {failed[:400]!r}")
        require("head-marker" in failed, f"Start of the failed output missing: {failed[:400]!r}")
        require("End of output (last " in failed and "tail-summary" in failed,
                f"End of the failed output missing: {failed[-800:]!r}")
        saved = re.search(r"Full output saved to: (\S+)", failed)
        require(saved is not None, f"Failed output names no saved file: {failed[:600]!r}")
        saved_text = Path(saved.group(1)).read_text(encoding="utf-8")
        require(saved_text.rstrip().endswith("tail-summary"),
                f"Saved failure output does not end with the real end: {saved_text[-200:]!r}")
        require(len(failed) < 10000, f"Large failure result was not kept short: {len(failed)} chars")

        passed = results["call_LARGEPASS0001"]
        require("start-marker" in passed and "End of output (last " in passed and
                "end-summary" in passed,
                f"Large output must show its start and end: {passed[:300]!r} ... {passed[-300:]!r}")
        require("Full output saved to: " in passed, f"Large output names no saved file: {passed[:400]!r}")

        timed = results["call_SLEEPTIME0001"]
        require("Command running in background" not in timed and "Exit code 143" in timed,
                f"A sleep command must stay in the foreground until its timeout: {timed!r}")
        require("Command timed out after" in timed,
                f"A timed-out command must say it timed out: {timed!r}")

        unknown = results["call_UNKNOWNFLD001"]
        require("Bash accepts only" in unknown and "cd <dir>" in unknown,
                f"An unknown Bash field must list the accepted fields: {unknown!r}")

        description = tools.get("Bash", "")
        require("start and end of the output" in description and "exit code" in description,
                f"Bash description must explain large and failed output: {description!r}")
    print("Bash results carry the output's end, the saved file, Batbox's notes and valid fields")


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))
