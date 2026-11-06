#!/usr/bin/env python3
"""File permission rules follow symlinks to the real path.

A deny rule on a directory holds when the Read, Write, Edit, Glob and Grep
Gadgets reach it through a symlink in the Roost. A symlink in the Roost that
points out of it is not treated as inside the Roost, and a Bash command that
changes into one is moved back. Before, deny rules matched only the path as
written, so a symlink walked past them.
"""
from pathlib import Path
import sys
import tempfile

from gadget_fixture import require, run


def main(binary):
    with tempfile.TemporaryDirectory(prefix="batbox-symlink-rules-") as temporary:
        root = Path(temporary).resolve()
        work = root / "work"
        (work / "inside").mkdir(parents=True)
        (work / "inside" / "plain.txt").write_text("PLAIN-CONTENT\n", encoding="utf-8")
        secret = root / "secret"
        secret.mkdir()
        (secret / "key.txt").write_text("TOPSECRET-VALUE\n", encoding="utf-8")
        outside = root / "outside"
        outside.mkdir()
        (outside / "loose.txt").write_text("OUTSIDE-CONTENT\n", encoding="utf-8")
        (work / "vault").symlink_to(secret, target_is_directory=True)
        (work / "escape").symlink_to(outside, target_is_directory=True)
        (work / "shortcut").symlink_to(work / "inside", target_is_directory=True)

        permissions = {"deny": [f"Read({secret}/**)", f"Edit({secret}/**)"],
                       "allow": ["Bash"]}
        vault = work / "vault"
        # Deny rules hold in every permission mode, so check them where nothing asks.
        denied_steps = [
            ("call_LINKREAD00001", "Read", {"file_path": str(vault / "key.txt")}),
            ("call_LINKGLOB00001", "Glob", {"pattern": "*.txt", "path": str(vault)}),
            ("call_LINKGREP00001", "Grep", {"pattern": "TOPSECRET", "path": str(vault)}),
            ("call_LINKWRITE0001", "Write", {"file_path": str(vault / "new.txt"),
                                             "content": "written through the link"}),
            ("call_LINKEDIT00001", "Edit", {"file_path": str(vault / "key.txt"),
                                            "old_string": "TOPSECRET", "new_string": "CHANGED"}),
        ]
        results, _ = run(binary, root, work, "denied", denied_steps, ["--yolo"],
                         client={"permissions": permissions},
                         environment={"HOME": str(root)})
        for call, _, _ in denied_steps:
            text = results[call]
            require("denied" in text.lower() and "TOPSECRET" not in text and
                    "key.txt" not in text.replace(str(vault / "key.txt"), ""),
                    f"{call}: a deny rule must hold through a symlink: {text!r}")
        require(not (secret / "new.txt").exists(), "Write created a file in a denied directory")
        require((secret / "key.txt").read_text(encoding="utf-8") == "TOPSECRET-VALUE\n",
                "Edit changed a file in a denied directory")

        boundary_steps = [
            ("call_LINKOUT000001", "Read", {"file_path": str(work / "escape" / "loose.txt")}),
            ("call_LINKIN0000001", "Read", {"file_path": str(work / "shortcut" / "plain.txt")}),
            ("call_LINKCDOUT0001", "Bash", {"command": "cd escape && pwd",
                                            "description": "Change into the escape link"}),
            ("call_LINKPWD000001", "Bash", {"command": "pwd", "description": "Print the cwd"}),
            ("call_LINKCDIN00001", "Bash", {"command": "cd shortcut && pwd",
                                            "description": "Change into the inside link"}),
            ("call_LINKPWD000002", "Bash", {"command": "pwd", "description": "Print the cwd"}),
        ]
        results, _ = run(binary, root, work, "boundary", boundary_steps,
                         ["--permission-mode", "default"],
                         client={"permissions": permissions},
                         environment={"HOME": str(root)})
        escaped = results["call_LINKOUT000001"]
        require("OUTSIDE-CONTENT" not in escaped and "Permission required" in escaped,
                f"A symlink out of the Roost must not count as inside it: {escaped!r}")
        require("PLAIN-CONTENT" in results["call_LINKIN0000001"],
                f"A symlink within the Roost reads as usual: {results['call_LINKIN0000001']!r}")

        require("Shell cwd was reset" in results["call_LINKCDOUT0001"],
                f"cd through a symlink out of the Roost must be undone: "
                f"{results['call_LINKCDOUT0001']!r}")
        require(results["call_LINKPWD000001"].strip() == str(work),
                f"The shell must be back in the Roost: {results['call_LINKPWD000001']!r}")
        require("Shell cwd was reset" not in results["call_LINKCDIN00001"],
                f"cd through a symlink within the Roost stays: {results['call_LINKCDIN00001']!r}")
        require(results["call_LINKPWD000002"].strip() == str(work / "inside"),
                f"The shell must stay in the linked folder: {results['call_LINKPWD000002']!r}")
    print("Permission rules and the Roost boundary follow symlinks to the real path")


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))
