#!/usr/bin/env python3
"""Skills found beside files the model touches load unless git ignores their folder.

Before, skill discovery was told every folder was not ignored, so a skill in an
ignored build or vendor folder loaded as soon as a file next to it was read.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

from gadget_fixture import require, run


def main(binary):
    with tempfile.TemporaryDirectory(prefix="batbox-skill-gitignore-") as temporary:
        root = Path(temporary).resolve()
        work = root / "work"
        work.mkdir()
        subprocess.run(["git", "init", "-q", str(work)], check=True)
        (work / ".gitignore").write_text("ignored/\n", encoding="utf-8")
        for name, folder in (("ignored-skill", "ignored"), ("tracked-skill", "tracked")):
            skill = work / folder / ".batbox" / "skills" / name
            skill.mkdir(parents=True)
            (skill / "SKILL.md").write_text(
                f"---\nname: {name}\ndescription: The {name} fixture\n---\nSay hello.\n",
                encoding="utf-8")
            (work / folder / "notes.txt").write_text("notes\n", encoding="utf-8")
        steps = [
            ("call_SKILLREAD0001", "Read", {"file_path": str(work / "ignored" / "notes.txt")}),
            ("call_SKILLREAD0002", "Read", {"file_path": str(work / "tracked" / "notes.txt")}),
            ("call_SKILLUSE00001", "Skill", {"skill": "tracked-skill"}),
            ("call_SKILLUSE00002", "Skill", {"skill": "ignored-skill"}),
        ]
        results, _ = run(binary, root, work, "skills", steps, ["--yolo"],
                         environment={"HOME": str(root)})
        tracked = results["call_SKILLUSE00001"]
        require("Unknown skill" not in tracked,
                f"A skill in a tracked folder must load: {tracked!r}")
        ignored = results["call_SKILLUSE00002"]
        require("Unknown skill: ignored-skill" in ignored,
                f"A skill in a git-ignored folder must not load: {ignored!r}")
    print("Skills in git-ignored folders are skipped; others load")


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))
