#!/usr/bin/env python3
"""What hooks write to BATBOX_ENV_FILE reaches every later Bash command.

A SessionStart hook exports a variable through its environment file, and a file
named by BATBOX_ENV_FILE when Batbox starts runs first. Before, the hooks wrote
their files and no Bash command read them.
"""
from pathlib import Path
import sys
import tempfile

from gadget_fixture import require, run


def main(binary):
    with tempfile.TemporaryDirectory(prefix="batbox-hook-environment-") as temporary:
        root = Path(temporary)
        work = root / "work"
        work.mkdir()
        parent = root / "parent-env.sh"
        parent.write_text("export BATBOX_PARENT_PROBE=from-parent\n", encoding="utf-8")
        hooks = {"SessionStart": [{"hooks": [{
            "type": "command",
            "command": "printf 'export BATBOX_HOOK_PROBE=from-session-start\\n' "
                       ">> \"$BATBOX_ENV_FILE\""}]}]}
        steps = [("call_HOOKENV000001", "Bash", {
            "command": "printf 'hook:%s parent:%s\\n' \"$BATBOX_HOOK_PROBE\" "
                       "\"$BATBOX_PARENT_PROBE\"",
            "description": "Print the hook variables"})]
        results, _ = run(binary, root, work, "environment", steps, ["--yolo"],
                         client={"hooks": hooks},
                         environment={"HOME": str(root), "BATBOX_ENV_FILE": str(parent)})
        output = results["call_HOOKENV000001"]
        require("hook:from-session-start" in output,
                f"The SessionStart hook's export did not reach Bash: {output!r}")
        require("parent:from-parent" in output,
                f"The BATBOX_ENV_FILE Batbox started with did not reach Bash: {output!r}")

        plain = root / "plain"
        plain.mkdir()
        steps = [("call_HOOKENV000002", "Bash", {"command": "printf 'plain:%s\\n' "
                                                 "\"$BATBOX_HOOK_PROBE\"",
                                                 "description": "Print without hooks"})]
        results, _ = run(binary, plain, work, "plain", steps, ["--yolo"],
                         environment={"HOME": str(plain)})
        require("plain:\n" in results["call_HOOKENV000002"] or
                results["call_HOOKENV000002"].strip() == "plain:",
                f"A Flight without hooks must not carry their variables: "
                f"{results['call_HOOKENV000002']!r}")
        require(not (plain / "plain-state" / "session-env").exists(),
                "A Flight without hook environment files must not create their folder")
    print("Hook environment files reach later Bash commands")


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))
